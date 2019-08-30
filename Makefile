campiano : campiano.cpp
	 g++ -std=c++11 `pkg-config opencv --cflags` campiano.cpp  `pkg-config opencv --libs` -lglut -lGL -lGLU -o campiano
CXXFLAGS=`pkg-config opencv --cflags`
LDLIBS=`pkg-config opencv --libs` -lglut -lGL -lGLU
