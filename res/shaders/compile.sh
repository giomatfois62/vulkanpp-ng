#!/bin/bash
for shader in $(ls *.vert *.frag); do
	glslangValidator $shader -V -o $shader.spv
done
