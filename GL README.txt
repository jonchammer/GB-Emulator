Glut libraries are in the GLUT folder in Libraries (Google Drive). They were copied into the MinGW64 folders so they would work.

glut.h is in C:/MinGW64/mingw64/include. It was copied manually. You have to set the "Include Directories" to look there to find it.

glut64.dll is in C:/MinGW64/mingw64/bin. It was put there manually. You shouldn't have to tell the IDE about it because that location is in the PATH.

All the libraries (*.lib) in 'libs' have to be included manually. It's in the linker settings under "Libraries" in Netbeans.

The windows GL libraries are out of date, so we have to use GLEW. GLEW replaces any GL/GLU includes, but the original libraries still have to be linked. glew.h is also in C:/MinGW64/mingw64/include. glew32.dll is in C:/MinGW64/mingw64/bin.