#version 330 core

out vec4 outCol;

uniform vec4 uCol; //uniform boja

void main()
{
	outCol = uCol;
	//outCol = vec4(1.0, 0.0, 0.0, 0.0);
}