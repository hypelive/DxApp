// DxApp.cpp : Определяет точку входа для приложения.
//

#include <chrono>

#include <d3d12.h>
#include <DirectXMath.h>

#include "framework.h"
#include "DxApp.h"
#include "Renderer.h"
#include "Scene.h"


#define MAX_LOADSTRING 100

struct AppState
{
	bool upPressed;
	bool downPressed;
	bool rightPressed;
	bool leftPressed;

	bool secondaryMBPressed;

	float mouseXPos;
	float mouseYPos;

	float mouseXPosDelta;
	float mouseYPosDelta;
};


static const char* kScenePath = "Scenes/Plane.glb";

// FULL HD
static constexpr uint32_t kWindowWidth = 1920;
static constexpr uint32_t kWindowHeight = 1080;

static constexpr float kCameraSpeed = 3.0f;
static constexpr float kCameraRotationSpeed = 0.01f;

static float GCurrentTime;
static float GDeltaTime;


// in seconds
static float GetTime()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	const auto currentTime = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
}


static void UpdateGlobalConstants()
{
	const auto currentTime = GetTime();
	GDeltaTime = currentTime - GCurrentTime;
	GCurrentTime = currentTime;
}


inline AppState* GetAppState(HWND hwnd)
{
	LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	AppState* pState = reinterpret_cast<AppState*>(ptr);
	return pState;
}


// Глобальные переменные:
HINSTANCE hInst; // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING]; // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING]; // имя класса главного окна

// Отправить объявления функций, включенных в этот модуль кода:
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int, AppState*);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Инициализация глобальных строк
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_DXAPP, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	AppState* pState = new AppState{};

	HWND hwnd = InitInstance(hInstance, nCmdShow, pState);

	if (hwnd == nullptr)
		return FALSE;

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DXAPP));

	auto* scene = new Scene(kScenePath);
	auto* baseRenderer = new Renderer(hwnd, kWindowWidth, kWindowHeight);
	baseRenderer->SetScene(scene);

	auto lastFrameTime = std::chrono::high_resolution_clock::now();

	MSG msg{};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT)
		{
			break;
		}

		UpdateGlobalConstants();

		AppState* pState = GetAppState(hwnd);
		if (pState)
		{
			const float cameraOffset = kCameraSpeed * GDeltaTime;

			Camera& camera = scene->GetCamera();
			XMFLOAT3 cameraTranslation;
			XMStoreFloat3(&cameraTranslation, XMVectorZero());
			if (pState->upPressed)
				cameraTranslation.z += cameraOffset;
			if (pState->downPressed)
				cameraTranslation.z -= cameraOffset;
			if (pState->rightPressed)
				cameraTranslation.x += cameraOffset;
			if (pState->leftPressed)
				cameraTranslation.x -= cameraOffset;

			camera.Translate(cameraTranslation);

			if (pState->secondaryMBPressed)
			{
				camera.Rotate(XMFLOAT2(pState->mouseXPosDelta * kCameraRotationSpeed, -pState->mouseYPosDelta * kCameraRotationSpeed));
				pState->mouseXPosDelta = 0.0f;
				pState->mouseYPosDelta = 0.0f;
			}

		}

		constexpr D3D12_VIEWPORT viewport = {
			0.0f,
			0.0f,
			static_cast<float>(kWindowWidth),
			static_cast<float>(kWindowHeight),
			0.0f,
			1.0f
		};

		baseRenderer->RenderScene(viewport);

		auto frameTime = std::chrono::high_resolution_clock::now();
		OutputDebugString(std::format(L"frameTime: {}\n", std::chrono::duration<float, std::chrono::milliseconds::period>(frameTime - lastFrameTime).count()).c_str());
		lastFrameTime = frameTime;
	}

	delete baseRenderer;
	delete scene;

	return static_cast<int>(msg.wParam);
}


//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DXAPP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DXAPP);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow, AppState* pState)
{
	hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, 0, kWindowWidth, kWindowHeight, nullptr, nullptr, hInstance, pState);

	if (hWnd)
	{
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
	}

	return hWnd;
}


//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	AppState* pState;
	if (message == WM_CREATE)
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		pState = reinterpret_cast<AppState*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
	}
	else
	{
		pState = GetAppState(hWnd);
	}

	float xPos, yPos;

	switch (message)
	{
	case WM_RBUTTONDOWN:
		SetCapture(hWnd);
		pState->secondaryMBPressed = true;

		xPos = static_cast<float>(GET_X_LPARAM(lParam));
		yPos = static_cast<float>(GET_Y_LPARAM(lParam));
		pState->mouseXPos = xPos;
		pState->mouseYPos = yPos;

		return 0;

	case WM_RBUTTONUP:
		ReleaseCapture();
		pState->secondaryMBPressed = false;
		return 0;

	case WM_MOUSEMOVE:

		if (pState->secondaryMBPressed)
		{
			xPos = static_cast<float>(GET_X_LPARAM(lParam));
			yPos = static_cast<float>(GET_Y_LPARAM(lParam));

			pState->mouseXPosDelta = xPos - pState->mouseXPos;
			pState->mouseYPosDelta = yPos - pState->mouseYPos;
			pState->mouseXPos = xPos;
			pState->mouseYPos = yPos;
		}

		return 0;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'W':
			pState->upPressed = true;
			break;
		case 'A':
			pState->leftPressed = true;
			break;
		case 'S':
			pState->downPressed = true;
			break;
		case 'D':
			pState->rightPressed = true;
			break;
		}

		return 0;

	case WM_KEYUP:
		switch (wParam)
		{
		case 'W':
			pState->upPressed = false;
			break;
		case 'A':
			pState->leftPressed = false;
			break;
		case 'S':
			pState->downPressed = false;
			break;
		case 'D':
			pState->rightPressed = false;
			break;
		}

		return 0;

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Разобрать выбор в меню:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}
