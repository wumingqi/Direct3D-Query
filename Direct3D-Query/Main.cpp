//Main.cpp	程序入口点

#include "pch.h"
#include "Application.h"
using namespace Query::Application;

_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow)
{
	return Application(L"学习Query Heap",hInstance).Run(nCmdShow);
}