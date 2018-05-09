#include "stdafx.h"

#include <string>
#include <iostream>

#include "TimeUtils.h"
#include "FormsUtils.h"

#include "Scene2DTest.h"
#include "Forms\Form.h"

void OpenConsole(void) {
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, 
	_In_ LPWSTR lpCmdLine,  _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	OpenConsole();

	InitTime();

	Form::GetInstance().Init(1280, 720, L"Form", L"Just a form", hInstance);
	Form::GetInstance().Show();

	LONGLONG LastTick = GetTime();

	// Main message loop:
	while (TRUE)
	{
		// wait for frame
		Form::GetInstance().WaitForNextFrame();

		// process input
		if (!ProcessMessages())
			break;

		// time calculation
		LONGLONG Now = GetTime();
		double dt = TimeToSeconds(Now - LastTick);
		LastTick = Now;

		// actual draw call
		Form::GetInstance().Tick(dt);
	}

	return 0;
}