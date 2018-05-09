#include "stdafx.h"

#include "Form.h"

#include "FormsUtils.h"
#include "TimeUtils.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <D3Dcompiler.h>

#include "WICTextureLoader.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Init

void Form::Init(unsigned int Width, unsigned int Height, const WCHAR *WindowClass, const WCHAR *Title,
	HINSTANCE hInstance) {

	// creating window

	RegisterClass(WindowClass, WndProcCallback, hInstance);

	POINT WindowPos;
	WindowPos.x = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
	WindowPos.y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;

	WindowHandle = CreateWindowW(WindowClass, Title, WS_POPUP, WindowPos.x, WindowPos.y, Width, Height, nullptr, nullptr, hInstance, nullptr);
	if (WindowHandle == 0)
		throw new runtime_error("Couldn't create window");

	// DirectX 11

	HRESULT res;

	// creating device and swap chain

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = Width;
	sd.BufferDesc.Height = Height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = WindowHandle;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, 0, 0/* | D3D11_CREATE_DEVICE_DEBUG*/, NULL, 0,
		D3D11_SDK_VERSION, &sd, &SwapChain, &DirectDevice, NULL, &DirectDeviceCtx);
	if (FAILED(res))
		throw new runtime_error("Couldn't create DX device and swap chain");

	// Setup the viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)Width;
	vp.Height = (float)Height;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	DirectDeviceCtx->RSSetViewports(1, &vp);

	// loading shader from resources
	HRSRC hShaderRes = FindResource(0, L"main_shader", RT_RCDATA);
	HANDLE hShaderResGlobal = LoadResource(0, hShaderRes);
	LPVOID ShaderData = LockResource(hShaderResGlobal);
	DWORD ShaderSize = SizeofResource(0, hShaderRes);

	// compiling and creating vertex shader
	ID3DBlob *Errors;

	ID3DBlob *VSBlob;
	res = D3DCompile(ShaderData, ShaderSize, NULL, NULL, NULL, "VS", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &VSBlob, &Errors);
	if (FAILED(res)) {
		string ErrorText = string((const char *)Errors->GetBufferPointer(), Errors->GetBufferSize());
		cout << ErrorText << endl;
		throw new runtime_error("Couldn't compile vertex shader");
	}

	res = DirectDevice->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), NULL, &VertexShader);
	if (FAILED(res))
		throw new runtime_error("Couldn't create vertex shader");

	// compiling and creating pixel shader
	ID3DBlob *PSBlob;
	res = D3DCompile(ShaderData, ShaderSize, NULL, NULL, NULL, "PS", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &PSBlob, &Errors);
	if (FAILED(res)) {
		string ErrorText = string((const char *)Errors->GetBufferPointer(), Errors->GetBufferSize());
		cout << ErrorText << endl;
		throw new runtime_error("Couldn't compile pixel shader");
	}

	res = DirectDevice->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), NULL, &PixelShader);
	if (FAILED(res))
		throw new runtime_error("Couldn't create pixel shader");

	// creating input layout
	static const D3D11_INPUT_ELEMENT_DESC VertexLayoutDesc[2] = {
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	res = DirectDevice->CreateInputLayout(VertexLayoutDesc, sizeof(VertexLayoutDesc) / sizeof(*VertexLayoutDesc),
		VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &VertexLayout);
	if (FAILED(res))
		throw new runtime_error("Couldn't create input layout");

	// Create a render target view
	ID3D11Texture2D *BackBuffer;
	res = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&BackBuffer);
	if (FAILED(res))
		throw new runtime_error("Couldn't get back buffer");

	res = DirectDevice->CreateRenderTargetView(BackBuffer, NULL, &RenderTargetView);
	if (FAILED(res))
		throw new runtime_error("Couldn't create render target");

	// creating depth stencil
	D3D11_TEXTURE2D_DESC DepthStencilDesc;

	DepthStencilDesc.Width = Width;
	DepthStencilDesc.Height = Height;
	DepthStencilDesc.MipLevels = 1;
	DepthStencilDesc.ArraySize = 1;
	DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	DepthStencilDesc.SampleDesc.Count = 1;
	DepthStencilDesc.SampleDesc.Quality = 0;

	DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthStencilDesc.CPUAccessFlags = 0;
	DepthStencilDesc.MiscFlags = 0;

	res = DirectDevice->CreateTexture2D(&DepthStencilDesc, 0, &DepthStencilBuffer);
	if (FAILED(res))
		throw new runtime_error("Couldn't create depth stencil buffer");

	res = DirectDevice->CreateDepthStencilView(DepthStencilBuffer, 0, &DepthStencilView);
	if (FAILED(res))
		throw new runtime_error("Couldn't create depth stencil view");

	// creating depth stencil states

	D3D11_DEPTH_STENCIL_DESC DepthStencilStateDesc;
	DepthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DepthStencilStateDesc.StencilEnable = FALSE;
	DepthStencilStateDesc.StencilReadMask = 0xFF;
	DepthStencilStateDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	DepthStencilStateDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilStateDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	DepthStencilStateDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilStateDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	DepthStencilStateDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilStateDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	DepthStencilStateDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilStateDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	DepthStencilStateDesc.DepthEnable = TRUE;
	res = DirectDevice->CreateDepthStencilState(&DepthStencilStateDesc, &DepthStencilState3D);
	if (FAILED(res))
		throw new runtime_error("Couldn't create depth stencil state");

	// setup cull modes
	D3D11_RASTERIZER_DESC CullModeDesc = { };
	CullModeDesc.CullMode = D3D11_CULL_NONE;
	CullModeDesc.FrontCounterClockwise = true;

	CullModeDesc.FillMode = D3D11_FILL_SOLID;
	res = DirectDevice->CreateRasterizerState(&CullModeDesc, &SolidMode);
	if (FAILED(res))
		throw new runtime_error("Couldn't create solid cullmode");

	DirectDeviceCtx->RSSetState(SolidMode);

	// creating samplers
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	res = DirectDevice->CreateSamplerState(&SamplerDesc, &SmoothSampler);
	if (FAILED(res))
		throw new runtime_error("Couldn't create smooth sampler");

	DirectDeviceCtx->PSSetSamplers(0, 1, &SmoothSampler);

	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	res = DirectDevice->CreateSamplerState(&SamplerDesc, &PointSampler);
	if (FAILED(res))
		throw new runtime_error("Couldn't create point sampler");

	DirectDeviceCtx->PSSetSamplers(1, 1, &PointSampler);

	// create shader variables references

	D3D11_BUFFER_DESC PersistentVertexVariablesDesc = { };
	PersistentVertexVariablesDesc.Usage = D3D11_USAGE_DEFAULT;
	PersistentVertexVariablesDesc.ByteWidth = sizeof(PersistentVertexVariables);
	PersistentVertexVariablesDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	res = DirectDevice->CreateBuffer(&PersistentVertexVariablesDesc, NULL, &PersistentVertexVariablesRef);
	if (FAILED(res))
		throw new runtime_error("Couldn't create persistent vertex variables ref");

	DirectDeviceCtx->VSSetConstantBuffers(0, 1, &PersistentVertexVariablesRef);

	D3D11_BUFFER_DESC PerDrawVertexVariablesDesc = {};
	PerDrawVertexVariablesDesc.Usage = D3D11_USAGE_DEFAULT;
	PerDrawVertexVariablesDesc.ByteWidth = sizeof(PerDrawVertexVariables);
	PerDrawVertexVariablesDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	res = DirectDevice->CreateBuffer(&PerDrawVertexVariablesDesc, NULL, &PerDrawVertexVariablesRef);
	if (FAILED(res))
		throw new runtime_error("Couldn't create per draw vertex variables ref");

	DirectDeviceCtx->VSSetConstantBuffers(1, 1, &PerDrawVertexVariablesRef);

	D3D11_BUFFER_DESC PersistentPixelVariablesDesc = { };
	PersistentPixelVariablesDesc.Usage = D3D11_USAGE_DEFAULT;
	PersistentPixelVariablesDesc.ByteWidth = sizeof(PersistentPixelVariables);
	PersistentPixelVariablesDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	res = DirectDevice->CreateBuffer(&PersistentPixelVariablesDesc, NULL, &PersistentPixelVariablesRef);
	if (FAILED(res))
		throw new runtime_error("Couldn't create persistent pixel variables ref");

	DirectDeviceCtx->PSSetConstantBuffers(0, 1, &PersistentPixelVariablesRef);

	D3D11_BUFFER_DESC PerDrawPixelVariablesDesc = {};
	PerDrawPixelVariablesDesc.Usage = D3D11_USAGE_DEFAULT;
	PerDrawPixelVariablesDesc.ByteWidth = sizeof(PerDrawPixelVariables);
	PerDrawPixelVariablesDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	res = DirectDevice->CreateBuffer(&PerDrawPixelVariablesDesc, NULL, &PerDrawPixelVariablesRef);
	if (FAILED(res))
		throw new runtime_error("Couldn't create PerDraw pixel variables ref");

	DirectDeviceCtx->PSSetConstantBuffers(1, 1, &PerDrawPixelVariablesRef);

	// setup frame
	DirectDeviceCtx->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

	DirectDeviceCtx->IASetInputLayout(VertexLayout);

	DirectDeviceCtx->VSSetShader(VertexShader, 0, 0);
	DirectDeviceCtx->PSSetShader(PixelShader, 0, 0);

	// register raw input

	RAWINPUTDEVICE Rid[1] = {};

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = RIDEV_INPUTSINK;   // adds HID mouse and also ignores legacy mouse messages
	Rid[0].hwndTarget = WindowHandle;

	if (!RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])))
		throw new runtime_error("Couldn't register raw devices");

	// setup matrices

	AspectRatio = (double)Width / (double)Height;
	ProjectionMatrix = XMMatrixPerspectiveFovLH((float)FOV, (float)AspectRatio, 0.1f, 1000.0f);

	// setup camera

	Up = XMVectorSet(0, 0, 1, 1);

	CameraAngle = { (float)(M_PI / 2), (float)(M_PI / 2) };

	BuildViewMatrix();
	UpdatePersistentVertexVariables();

	// setup planes

	GeneratePlanes();
}

// Window and Input events processing

void Form::ProcessWindowState(void)
{
	bool IsInFocusNow = GetForegroundWindow() == WindowHandle;

	if (IsInFocusNow != IsInFocus) {

		IsInFocus = IsInFocusNow;

		if (IsInFocus) {
			RECT Rect;
			POINT Center;

			GetWindowRect(WindowHandle, &Rect);
			Center = { Rect.left + (Rect.right - Rect.left) / 2, Rect.top + (Rect.bottom - Rect.top) / 2 };
			Rect = { Center.x, Center.y, Center.x + 1, Center.y + 1 };
			ClipCursor(&Rect);
			ShowCursor(false);
		}
		else {
			ShowCursor(true);
			ClipCursor(NULL);
		}
	}
}

void Form::ProcessMouseInput(LONG dx, LONG dy) {

	CameraAngle.x -= (float)dx / 300.0f;
	CameraAngle.y += (float)dy / 300.0f;

	CameraAngle.x /= (float)M_PI * 2.0f;
	CameraAngle.x -= (float)(long)CameraAngle.x;
	CameraAngle.x *= (float)M_PI * 2.0f;

	CameraAngle.y = min(max(0.1f, CameraAngle.y), 3.13f);

	BuildViewMatrix();
	UpdatePersistentVertexVariables();
}

void Form::ProcessKeyboardInput(double dt)
{
	XMVECTOR SideVector = XMVector3Normalize(XMVector3Cross(-TargetVector, Up));
	XMVECTOR ForwardVector = XMVector3Cross(SideVector, Up);

	XMVECTOR Displacement = {};

	if (PressedKeys['D'])
		Displacement += SideVector;
	if (PressedKeys['A'])
		Displacement -= SideVector;
	if (PressedKeys['W'])
		Displacement += ForwardVector;
	if (PressedKeys['S'])
		Displacement -= ForwardVector;
	if (PressedKeys[VK_SHIFT])
		Displacement -= Up;
	if (PressedKeys[VK_SPACE] || PressedKeys[VK_CONTROL])
		Displacement += Up;

	Displacement = XMVector3Normalize(Displacement) * (float)(MoveSpeed * dt);

	if (XMVector3LengthSq(Displacement).m128_f32[0] <= 0.0)
		return;

	XMVECTOR Position = XMLoadFloat3(&CameraPosition);
	Position += Displacement;
	XMStoreFloat3(&CameraPosition, Position);

	BuildViewMatrix();
	UpdatePersistentVertexVariables();
}

void Form::Show(void) {

	ShowWindow(WindowHandle, SW_SHOW);
}

bool Form::HandleKeyPress(int Key)
{
	switch (Key) {
	case  VK_ESCAPE:
		PostQuitMessage(0);
		break;
	case 'V':
		UseVSync = !UseVSync;
		break;
	default:
		return false;
	}

	return true;
}

// Window Callback

LRESULT Form::WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INPUT:
		UINT RimType;
		HRAWINPUT RawHandle;
		RAWINPUT RawInput;
		UINT RawInputSize, Res;

		RimType = GET_RAWINPUT_CODE_WPARAM(wParam);
		RawHandle = HRAWINPUT(lParam);

		RawInputSize = sizeof(RawInput);
		Res = GetRawInputData(RawHandle, RID_INPUT, &RawInput, &RawInputSize, sizeof(RawInput.header));

		if (Res != (sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE)))
			break;

		if ((RawInput.data.mouse.usFlags & 1) != MOUSE_MOVE_RELATIVE)
			break;

		if (IsInFocus)
			ProcessMouseInput(RawInput.data.mouse.lLastX, RawInput.data.mouse.lLastY);

		break;
	case WM_KEYDOWN: {
		int Key = (int)wParam;

		if (!HandleKeyPress(Key) && Key < KEYS_COUNT)
			PressedKeys[Key] = true;
		break;
	}
	case WM_KEYUP:
		if (wParam < KEYS_COUNT)
			PressedKeys[wParam] = false;
		break;
	case WM_MOUSEWHEEL: {

		float Mul = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1.1f : 1.0f / 1.1f;

		MoveSpeed = max(1.0f, MoveSpeed * Mul);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
	return 0;
}

LRESULT Form::WndProcCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return Form::GetInstance().WndProc(hWnd, Msg, wParam, lParam);
}

// 3D Utils

void Form::UpdateAllPerDrawVariables(void)
{
	XMMATRIX InvWorldMatrix = XMMatrixTranspose(WorldMatrix);
	XMStoreFloat4x4(&PerDrawVertexVariables.WorldMatrix, InvWorldMatrix);

	DirectDeviceCtx->UpdateSubresource(PerDrawVertexVariablesRef, 0, NULL, &PerDrawVertexVariables, 0, 0);

	DirectDeviceCtx->UpdateSubresource(PerDrawPixelVariablesRef, 0, NULL, &PerDrawPixelVariables, 0, 0);
}

void Form::BuildViewMatrix(void)
{
	float x = sinf(CameraAngle.y) * sinf(CameraAngle.x);
	float y = sinf(CameraAngle.y) * cosf(CameraAngle.x);
	float z = cosf(CameraAngle.y);

	TargetVector = XMVectorSet(x, y, z, 1);

	XMVECTOR Position = XMVectorSet(CameraPosition.x, CameraPosition.y, CameraPosition.z, 1);
	XMVECTOR Target = Position + TargetVector;

	ViewMatrix = XMMatrixLookAtLH(Position, Target, Up);
}

void Form::UpdatePersistentVertexVariables(void)
{
	XMMATRIX FinalMatrix = XMMatrixTranspose(ViewMatrix * ProjectionMatrix);
	XMStoreFloat4x4(&PersistentVertexVariables.FinalMatrix, FinalMatrix);

	DirectDeviceCtx->UpdateSubresource(PersistentVertexVariablesRef, 0, NULL, &PersistentVertexVariables, 0, 0);
}

void Form::GeneratePlanes(void)
{
	HRESULT res;

	Vertex PlanesData[4];
	
	float X = (float)(1.0 / (AspectRatio * 2.0 * tan(FOV / 2.0)));

	PlanesData[0].Pos = { X, -0.5f, -0.28125f };
	PlanesData[1].Pos = { X,  0.5f, -0.28125f };
	PlanesData[2].Pos = { X, -0.5f,  0.28125f };
	PlanesData[3].Pos = { X,  0.5f,  0.28125f }; // suchara

	PlanesData[0].Tex = { 0, 1 };
	PlanesData[1].Tex = { 1, 1 };
	PlanesData[2].Tex = { 0, 0 };
	PlanesData[3].Tex = { 1, 0 };

	D3D11_SUBRESOURCE_DATA InitialData = { };

	// creating buffers
	D3D11_BUFFER_DESC VertexBufferDesc = {};
	VertexBufferDesc.ByteWidth = sizeof(PlanesData);
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	InitialData.pSysMem = PlanesData;
	res = DirectDevice->CreateBuffer(&VertexBufferDesc, &InitialData, &PlanesBuffer);
	if (FAILED(res))
		throw new runtime_error("Couldn't create vertex buffer");

	VertexCount = sizeof(PlanesData) / sizeof(PlanesData[0]);

	CoInitialize(NULL);

	res = CreateWICTextureFromFile(DirectDevice, L"..\\data\\background.jpg", (ID3D11Resource**)&Texture, &TextureView);
	if (FAILED(res))
		throw new runtime_error("Couldn't load texture");
}

void Form::WaitForNextFrame(void)
{
	HRESULT res;

	if (SyncQuery == NULL) {
		D3D11_QUERY_DESC QueryDesc = {};
		QueryDesc.Query = D3D11_QUERY_EVENT;

		res = DirectDevice->CreateQuery(&QueryDesc, &SyncQuery);
		if (FAILED(res))
			throw new runtime_error("Couldn't create sync query");
	}
	else {

		BOOL QueryResult;
		do {
			res = DirectDeviceCtx->GetData(SyncQuery, &QueryResult, sizeof(QueryResult), 0);
			if (res == S_FALSE)
				QueryResult = false;
			else
				if (res != S_OK)
					throw new runtime_error("Sync query wait failed");
		} while (!QueryResult);
	}
}

float BoolToFloat(bool B) {
	
	return B ? 1.0f : 0.0f;
}

void Form::DrawScene(void)
{
	// clear the back buffer to a deep blue
	float color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	DirectDeviceCtx->ClearRenderTargetView(RenderTargetView, color);
	DirectDeviceCtx->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	WorldMatrix = XMMatrixIdentity();
	UpdateAllPerDrawVariables();

	DirectDeviceCtx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	DirectDeviceCtx->OMSetDepthStencilState(DepthStencilState3D, 0);

	DirectDeviceCtx->PSSetShaderResources(0, 1, &TextureView);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	DirectDeviceCtx->IASetVertexBuffers(0, 1, &PlanesBuffer, &stride, &offset);

	DirectDeviceCtx->Draw(VertexCount, 0);

	if (SyncQuery != NULL)
		DirectDeviceCtx->End(SyncQuery);

	// switch the back buffer and the front buffer
	SwapChain->Present(UseVSync ? 1 : 0, 0);
}

void Form::Tick(double dt) {

	ProcessWindowState();
	ProcessKeyboardInput(dt);
	DrawScene();
}