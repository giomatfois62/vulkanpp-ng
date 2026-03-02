#ifndef ECS_HPP
#define ECS_HPP

#include <cstdint>
#include <cassert>
#include <cstdio>
#include <utility>
#include <functional>
#include <limits>
#include <algorithm>
#include <vector>
#include <queue>
#include <map>
#include <typeinfo> // operator typeid

namespace ecs {

typedef uint16_t ComponentType;
typedef size_t Entity;

template <class T>
class _Component {
public:
    static ComponentType type() { return T::m_type; }

    Entity id() const { return m_id; }

    void setId(Entity id) { m_id = id; }

    _Component<T>& operator=(const _Component<T> &other) { return *this; } // prevent id copy

private:
	const static ComponentType m_type;
	Entity m_id = 0;
};

class ECS;

struct ComponentDescription {
	const char* label;
	ComponentType type;

	std::function<void(ECS &world, Entity id)> add;
	std::function<void(ECS &world, Entity id)> remove;
    std::function<void(ECS &world, Entity id, Entity other)> copy;
    std::function<void(ECS &world, Entity id, Entity other)> swap;
	std::function<void(ECS &world, Entity id)> drawUI;
};

inline std::vector<ComponentDescription> Components; // c++17

template<class T>
ComponentType registerComponent()
{
	Components.push_back({
		T::name,
		static_cast<ComponentType>(Components.size()),
		T::add,
		T::remove,
		T::copy,
		T::swap,
		T::drawUI
	});

	assert(Components.size() < std::numeric_limits<ComponentType>::max());

	return Components.size()-1;
}

template <class T>
const ComponentType _Component<T>::m_type = registerComponent<T>();

class BaseContainer {
public:
	virtual ~BaseContainer() = default;

	virtual size_t size() = 0;
	virtual void clear() = 0;
	virtual std::pair<size_t, size_t> remove(size_t index) = 0;
	virtual std::vector<size_t>& ids() = 0;
};

template<class T>
class Container : public BaseContainer {
public:
    Container() { m_items.resize(1); m_ids.clear(); } // TODO: consider calling reserve

    size_t size() override { return m_items.size() - 1; }
    std::vector<size_t>& ids() override { return m_ids; }
    void clear() override { m_items.resize(1); m_ids.clear(); }

	T& itemAt(size_t index) { return m_items[index]; }
	T& operator[](size_t index) { return m_items[index]; }
    std::vector<T>& items() { return m_items; }

    typename std::vector<T>::iterator begin() { return m_items.begin() + 1; }
    typename std::vector<T>::iterator end()   { return m_items.end(); }
    typename std::vector<T>::iterator cbegin() { return m_items.begin() + 1; }
    typename std::vector<T>::iterator cend()   { return m_items.end(); }

	size_t insert(const T &item)
	{
		m_items.push_back(item);
		m_ids.push_back(item.id());

		return m_items.size() - 1;
	}

    std::pair<ecs::Entity, size_t> remove(size_t index) override
	{
		if (index < m_items.size() - 1) {
			std::swap(m_items[index], m_items[m_items.size() - 1]);
			m_items.pop_back();

            std::swap(m_ids[index], m_ids[m_ids.size() - 1]);
            m_ids.pop_back();

			return std::make_pair(m_items[index].id(), index);
		} else {
			m_items.pop_back();
			m_ids.pop_back();

			return std::make_pair(0, 0);
		}
	}

private:
    std::vector<T> m_items;
    std::vector<size_t> m_ids;
};

template<class T>
class SparseContainer {
public:
    SparseContainer() { m_items.resize(1); } // TODO: consider calling reserve

	size_t realSize() { return m_items.size(); }
	size_t size() { return m_items.size() - m_free.size(); }
	T& itemAt(size_t index) { return m_items[index]; }
	T& operator[](size_t index) { return m_items[index]; }
	std::vector<T> &items() { return m_items; }
	void clear() { m_items.resize(1); }

	size_t insert(const T &item)
	{
		size_t itemIndex;

		if (!m_free.empty()) {
			itemIndex = m_free.front();
			m_free.pop();
			m_items[itemIndex] = std::move(item);
		} else {
			itemIndex = m_items.size();
			m_items.push_back(std::move(item));
		}

		return itemIndex;
	}

	void remove(size_t index)
	{
		if (index < m_items.size() - 1) {
			m_free.push(index);
			m_items[index] = T();
		} else {
			m_items.pop_back();
		}
	}

private:
	std::vector<T> m_items;
	std::queue<size_t> m_free;
};

class ComponentStorage {
public:
	ComponentStorage()
	{
		m_storage.resize(Components.size());
	}

	~ComponentStorage() { clear(); }

	BaseContainer* operator[](size_t index)
	{
		return m_storage[index];
	}

	template <typename T>
    Container<T>* get()
	{
		ComponentType type(T::type());

		if (m_storage[type] == nullptr)
			m_storage[type] = new Container<T>();

		return static_cast<Container<T>*>(m_storage[type]);
	}

	BaseContainer* get(ComponentType type)
	{
		return m_storage[type];
	}

	void clear()
	{
		for(BaseContainer* b : m_storage)
			delete b;

		m_storage.clear();
	}

private:
	std::vector<BaseContainer*> m_storage;
};

typedef uint16_t SystemType;

class BaseSystem {
public:
	virtual ~BaseSystem() {}

	virtual void update(ECS &world, float dt) = 0;
	virtual const char* getName() = 0;
	virtual SystemType getType() = 0;

    void setEnabled(bool value) { m_enabled = value; }
    bool isEnabled() { return m_enabled; }

private:
    bool m_enabled = true;
};

template <class T>
class _System : public BaseSystem {
public:
	const char* getName() { return T::name; }

	static SystemType type() { return T::m_type; }

	SystemType getType() { return T::m_type; }

private:
	const static SystemType m_type;
};

struct SystemDescription {
	const char* label;
	SystemType type;

    std::function<void(ECS &world)> add;
    std::function<void(ECS &world)> remove;
    std::function<void(ECS &world)> drawUI;
};

inline std::vector<SystemDescription> Systems; // c++17

template<class T>
SystemType registerSystem()
{
    Systems.push_back({
        T::name,
        static_cast<SystemType>(Systems.size()),
        T::add,
        T::remove,
        T::drawUI
    });

    assert(Systems.size() < std::numeric_limits<SystemType>::max());

    return Systems.size()-1;
}

template<class T>
const SystemType _System<T>::m_type = registerSystem<T>();

typedef std::vector<size_t> ComponentList;
typedef SparseContainer<ComponentList> EntityList;
typedef std::vector<BaseSystem*> SystemList;

// https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
inline std::size_t hash_components(std::vector<ComponentType> const& vec)
{
	std::size_t seed = vec.size();

	for(auto& i : vec)
		seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);

	return seed;
}

typedef std::map<size_t, std::vector<Entity>> EntityCache;

struct CachePointer {
	bool valid = false;
	EntityCache::iterator iterator;
};

class ECS {
public:
	ECS()
	{
		m_entities[0] = ComponentList(Components.size(), 0);
		m_cachePointers.resize(Components.size());
	}

	~ECS()
	{
		for (auto &system : m_systems)
			delete system;
	}

    EntityList& entities() { return m_entities; }

    SystemList& systems() { return m_systems; }

	template <class T>
	void addComponent(Entity id, T&& component)
	{
		ComponentType type(T::type());

		component.setId(id);

		size_t index = m_entities[id][type];

        if (index > 0 || id == 0) {
			m_components.get<T>()->itemAt(index) = component;
		} else {
			m_entities[id][type] = m_components.get<T>()->insert(component);

			for (auto &pair : m_cachePointers[type])
				pair.second.valid = false;
		}

		T::onAdd(*this, id);
	}

	template <class T>
	void removeComponent(Entity id)
	{
        T::onRemove(*this, id);

		ComponentType type(T::type());

		auto pair = m_components.get(type)->remove(componentIndex(id, type));

		if (pair.first > 0)
			m_entities[pair.first][type] = pair.second;

		m_entities[id][type] = 0;

        for (auto &pair : m_cachePointers[type])
			pair.second.valid = false;
	}

	template <class T>
    Container<T>& components()
    {
        return (*m_components.get<T>());
	}

	template <class T>
	T& componentWithIndex(size_t index)
	{
		return m_components.get<T>()->itemAt(index);
	}

	template <class T>
    T& component(Entity id = 0)
	{
		return componentWithIndex<T>(componentIndex(id, T::type()));
	}

	Entity createEntity()
	{
		return m_entities.insert(ComponentList(Components.size(), 0));
	}

	void destroyEntity(Entity id)
	{
		for(size_t i = 0; i < m_entities[id].size(); ++i) {
			if (m_entities[id][i] == 0)
				continue;

			auto pair = m_components.get(i)->remove(componentIndex(id, i));

			if (pair.first > 0)
				m_entities[pair.first][i] = pair.second;
		}

		m_entities.remove(id);
	}

	template<typename T>
	std::vector<size_t>&  entitiesWithComponent()
	{
        return m_components.get<T>()->ids();
	}

	std::vector<size_t>&  entitiesWithComponent(ComponentType type)
	{
		static std::vector<size_t> empty = {};

		if (!m_components.get(type))
			return empty;

		return m_components.get(type)->ids();
	}

	std::vector<size_t>& entitiesWithComponents(std::vector<ComponentType> &list)
	{
		static std::vector<size_t> empty = {};

		for (auto &type : list) {
			if (!m_components.get(type))
				return empty;
		}

		auto refreshCache = [&](){
			std::vector<size_t> entities = {};

			for (Entity id : m_components[list[0]]->ids()) {
				if (hasComponents(id, list))
					entities.push_back(id);
			}

			return entities;
		};

		std::sort(list.begin(), list.end(),
		[this](auto c1, auto c2) {
			return m_components[c1]->size() < m_components[c2]->size();
		});

		size_t hash = hash_components(list);

		auto it = m_cache.find(hash);

		if (it == m_cache.end()) {
			it = m_cache.insert(std::make_pair(hash, std::vector<Entity>())).first;

			for (auto &type : list) // add hash to components hashes list
				m_cachePointers[type].insert({hash, {true, it}});

			it->second = refreshCache();

		} else {
			for (size_t i = 0; i < list.size(); ++i) {
				if (!m_cachePointers[list[i]][hash].valid) {
					it->second = refreshCache();

					for (size_t j = i; j < list.size(); ++j)
						m_cachePointers[list[j]][hash].valid = true;

					break;
				}
			}
		}

		return it->second;
	}

	template<typename... Targs>
	std::vector<size_t>& entitiesWithComponents()
	{
		std::vector<ComponentType> list;
		readComponents<Targs...>(list);

		return entitiesWithComponents(list);
	}

	void cleanUp()
	{
		m_entities.clear();
		m_components.clear();
	}

	size_t componentIndex(Entity id, ComponentType type)
	{
		if (m_entities[id].size() <= type)
			m_entities[id].resize(type + 1, 0);

		return m_entities[id][type];
	}

	template<class T>
	bool hasComponents(Entity id)
	{
		auto &indices = m_entities[id];

		if (indices.size())
			return indices[T::type()];
		else
			return false;
	}

	template<class T1, class T2, class ...Args>
	bool hasComponents(Entity id)
	{
		if (!hasComponents<T1>(id))
			return false;

		return hasComponents<T2, Args...>(id);
	}

	bool hasComponents(Entity id, const std::vector<ComponentType> &list)
	{
		auto &indices = m_entities[id];

		if (!indices.size())
			return false;

		for (auto & component : list) {
			if (indices[component] == 0)
				return false;
		}

		return true;
	}

    template<class T>
    void addSystem()
    {
        for (auto &system : m_systems) {
            if (system->getType() == T::type())
                return;
        }

        m_systems.push_back(new T());

        T::onAdd(*this);
    }

	template<class T>
	void removeSystem()
	{
        for (auto it = m_systems.begin(); it != m_systems.end(); ++it) {
			if ((*it)->getType() == T::type()) {
                T::onRemove(*this);

				delete (*it);
                m_systems.erase(it);

				return;
			}
		}
	}

	template<class T>
	T* system()
	{
        for (auto &system : m_systems)
            if (system->getType() == T::type())
                return static_cast<T*>(system);

		return nullptr;
	}

    template<class T>
    void enableSystem()
    {
        for (auto &system : m_systems) {
            if (system->getType() == T::type()) {
                system->setEnabled(true);
                return;
            }
        }
    }

    template<class T>
    void disableSystem()
    {
        for (auto &system : m_systems) {
            if (system->getType() == T::type()) {
                system->setEnabled(false);
                return;
            }
        }
    }

	void update(float dt) {
		for (auto &system : m_systems)
            if (system->isEnabled())
                system->update(*this, dt);
	}

private:
	EntityList m_entities;
	ComponentStorage m_components;
    SystemList m_systems;
	std::vector<std::map<size_t, CachePointer>> m_cachePointers;
    EntityCache m_cache;

	template<class T>
	void readComponents(std::vector<ComponentType> &list)
	{
		list.push_back(T::type());
	}

	template<class T1, class T2, class ...Args>
	void readComponents(std::vector<ComponentType> &list)
	{
		readComponents<T1>(list);
		readComponents<T2, Args...>(list);
	}
};

template <class T>
class Component : public _Component<T> {
public:
    static constexpr const char *name = "Unknown Component";

	static void drawUI(ECS &world, Entity id) {}

	static void add(ECS &world, Entity id)
	{
		world.addComponent(id,T());
	}

	static void remove(ECS &world, Entity id)
	{
		world.removeComponent<T>(id);
	}

	static void copy(ECS &world, Entity id, Entity other)
	{
		world.component<T>(id) = world.component<T>(other);
	}

	static void swap(ECS &world, Entity id, Entity other)
	{
		T tmp = world.component<T>(id);
		world.component<T>(id) = world.component<T>(other);
		world.component<T>(other) = tmp;
	}

    static void onAdd(ECS &/*world*/, Entity /*id*/) {}
    static void onRemove(ECS &/*world*/, Entity /*id*/) {}
};

template<class T>
class System : public _System<T> {
public:
    static constexpr const char *name = "Unknown System";

	static void drawUI(ECS &world) { }

    static void add(ECS &world)
    {
        world.addSystem<T>();
    }

    static void remove(ECS &world)
    {
        world.removeSystem<T>();
    }

    static void onAdd(ECS &/*world*/) {}
    static void onRemove(ECS &/*world*/) {}
};

} // namespace ecs

#endif
