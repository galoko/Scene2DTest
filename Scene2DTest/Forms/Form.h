#include "stdafx.h"

#include <inttypes.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <string>
#include <array>
#include <stack>

#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;
using namespace std;

struct Vertex {
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
};

class Form {
public:
	static Form& GetInstance() {
		static Form instance;

		return instance;
	}

	Form(Form const&) = delete;
	void operator=(Form const&) = delete;
private:
	Form(void) { };

	// Window

	HWND WindowHandle;

	// DirectX 11 

	ID3D11Device *DirectDevice;
	ID3D11DeviceContext *DirectDeviceCtx;
	IDXGISwapChain *SwapChain;
	ID3D11RenderTargetView *RenderTargetView;

	ID3D11VertexShader *VertexShader;
	ID3D11PixelShader *PixelShader;

	ID3D11InputLayout *VertexLayout;

	ID3D11Texture2D *DepthStencilBuffer;
	ID3D11DepthStencilView *DepthStencilView;

	ID3D11DepthStencilState *DepthStencilState3D;

	XMMATRIX WorldMatrix;

	XMMATRIX ViewMatrix;
	XMMATRIX ProjectionMatrix;

	struct {
		XMFLOAT4X4 FinalMatrix;
	} PersistentVertexVariables;
	ID3D11Buffer *PersistentVertexVariablesRef;

	struct {
		XMFLOAT4X4 WorldMatrix;
	} PerDrawVertexVariables;
	ID3D11Buffer *PerDrawVertexVariablesRef;

	struct {
		XMFLOAT4X4 Placeholder1;
	} PersistentPixelVariables;
	ID3D11Buffer *PersistentPixelVariablesRef;

	struct {
		XMFLOAT4X4 Placeholder2;
	} PerDrawPixelVariables;
	ID3D11Buffer *PerDrawPixelVariablesRef;

	ID3D11SamplerState *PointSampler;
	ID3D11SamplerState *SmoothSampler;

	ID3D11RasterizerState *SolidMode;

	// for input lag reduction
	ID3D11Query *SyncQuery;

	// objects

	ID3D11Buffer *PlanesBuffer;
	uint32_t VertexCount;

	ID3D11Texture2D *Texture;
	ID3D11ShaderResourceView *TextureView;

	// GUI

	XMFLOAT3 CameraPosition;
	XMFLOAT2 CameraAngle;
	XMVECTOR TargetVector;
	XMVECTOR Up;

	bool IsInFocus;
	bool UseVSync = true;

	double FOV = 0.25 * M_PI;
	double AspectRatio;

#define KEYS_COUNT 256
	bool PressedKeys[KEYS_COUNT];

	float MoveSpeed = 1.0f;

	// Window and Input events processing

	void ProcessWindowState(void);
	void ProcessMouseInput(LONG dx, LONG dy);
	void ProcessKeyboardInput(double dt);

	bool HandleKeyPress(int Key);

	// Window Callback

	LRESULT WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProcCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	// 3D Utils

	void UpdateAllPerDrawVariables(void);

	void BuildViewMatrix(void);
	void UpdatePersistentVertexVariables(void);

	void GeneratePlanes(void);

	void DrawScene(void);
public:
	void Init(unsigned int Width, unsigned int Height, const WCHAR *WindowClass, const WCHAR *Title, HINSTANCE hInstance);
	void Show(void);
	void WaitForNextFrame(void);
	void Tick(double dt);
};