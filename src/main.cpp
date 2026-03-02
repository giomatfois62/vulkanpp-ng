#include "application.hpp"

#include <stdexcept>
#include <iostream>

using namespace std;

int main(int argc, char **argv)
{
	Application app(argc, argv);

    app.setApplicationName("Vulkan Application");
	app.setWindowTitle("Hello Vulkan!");

	try {
		app.run();
	} catch (const std::runtime_error &e) {
		cerr << "Runtime Error: " << e.what() << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
