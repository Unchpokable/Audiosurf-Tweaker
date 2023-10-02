#include <stdexcept>
#include <algorithm>
#include "dllmain.h"

using namespace OverlaySpecs;

#pragma region DXFunctions impl
DXFunctions::DXFunctions()
{
	m_pEndScene = nullptr;
	m_pReset = nullptr;
}


DXFunctions::DXFunctions(DxEndScene pEndScene, DxReset pReset)
{
	if (pEndScene == nullptr || pReset == nullptr)
		throw std::invalid_argument("One of the required DirectX functions was assigned to nullptr");

	m_pEndScene = pEndScene;
	m_pReset = pReset;
}

DxEndScene DXFunctions::GetEndScene() const noexcept
{
	return m_pEndScene;
}

DxReset DXFunctions::GetReset() const noexcept
{
	return m_pReset;
}

void DXFunctions::SetEndScene(DxEndScene pEndScene)
{
	THROW_IF_NULL(pEndScene, "DxEndScene")
		m_pEndScene = pEndScene;
}

void DXFunctions::SetReset(DxReset pReset)
{
	THROW_IF_NULL(pReset, "DxReset")
	m_pReset = pReset;
}
#pragma endregion


#pragma region DXParameters impl
PD3DPRESENT_PARAMETERS DXParameters::GetPresentParameters() const noexcept
{
	return m_pD3DPresentParameters;
}

LPDIRECT3DDEVICE9 DXParameters::GetDevice() const noexcept
{
	return m_pCurrentD3DDevice9;
}

LPD3DXFONT DXParameters::GetFont() const noexcept
{
	return m_pFont;
}

void DXParameters::SetPresentParameters(PD3DPRESENT_PARAMETERS pParams)
{
	m_pD3DPresentParameters = pParams;
}

void DXParameters::SetDevice(LPDIRECT3DDEVICE9 pDevice)
{
	THROW_IF_NULL(pDevice, "Direct3DDevice9 Interface")
		m_pCurrentD3DDevice9 = pDevice;
}

void DXParameters::SetFont(LPD3DXFONT pFont)
{
	THROW_IF_NULL(pFont, "Direct3D Font")
		m_pFont = pFont;
}
#pragma endregion

#pragma region OverlayMenuParameters impl
PArgb_t OverlayMenuParameters::GetFontColor() const noexcept
{
	return m_fontColor;
}

int OverlayMenuParameters::GetFontSize() const noexcept
{
	return m_fontSize;
}

float OverlayMenuParameters::GetMenuFontSize() const noexcept
{
	return m_menuFontSize;
}

LPCSTR OverlayMenuParameters::GetFontFamily() const noexcept
{
	return m_fontFamily;
}

void OverlayMenuParameters::SetFontColor(PArgb_t newFontColor)
{
	if (newFontColor == nullptr)
	{
		throw std::invalid_argument("FontColor cannot be nullptr");
	}
	m_fontColor = newFontColor;
}

void OverlayMenuParameters::SetFontSize(int newFontSize)
{
	m_fontSize = newFontSize;
}


void OverlayMenuParameters::SetMenuFontSize(float newMenuFontSize)
{
	m_menuFontSize = newMenuFontSize;
}

void OverlayMenuParameters::SetFontFamily(LPCSTR family)
{
	m_fontFamily = family;
}

#pragma endregion

#pragma region OverlayInfopanelParameters impl
int OverlayInfopanelParameters::GetLeftXPos() const noexcept
{
	return m_leftXPos;
}
int OverlayInfopanelParameters::GetLeftYPos() const noexcept
{
	return m_leftYPos;
}
int OverlayInfopanelParameters::GetWidth() const noexcept
{
	return m_width;
}
int OverlayInfopanelParameters::GetHeight() const noexcept
{
	return m_height;
}
int* OverlayInfopanelParameters::GetOverlayXOffset() const noexcept
{
	return m_overlayXOffset;
}
int* OverlayInfopanelParameters::GetOverlayYOffset() const noexcept
{
	return m_overlayYOffset;
}

void OverlayInfopanelParameters::SetLeftXPos(int newLeftXPos)
{
	m_leftXPos = newLeftXPos;
}

void OverlayInfopanelParameters::SetLeftYPos(int newLeftYPos)
{
	m_leftYPos = newLeftYPos;
}

void OverlayInfopanelParameters::SetWidth(int newWidth)
{
	m_width = newWidth;
}

void OverlayInfopanelParameters::SetHeight(int newHeight)
{
	m_height = newHeight;
}

void OverlayInfopanelParameters::SetOverlayXOffset(int* newOffset)
{
	THROW_IF_NULL(newOffset, "OverlayXOffset")
		delete m_overlayXOffset;
	m_overlayXOffset = newOffset;
}

void OverlayInfopanelParameters::SetOverlayYOffset(int* newOffset)
{
	THROW_IF_NULL(newOffset, "OverlayYOffset")
		delete m_overlayYOffset;
	m_overlayYOffset = newOffset;
}

#pragma endregion

#pragma region OverlayMenuParameters impl
OverlayMenuParameters* OverlayParameters::GetMenuParameters() const noexcept
{
	return m_menuParameters;
}

OverlayInfopanelParameters* OverlayParameters::GetInfopanelParameters() const noexcept
{
	return m_infopanelParameters;
}

void OverlayParameters::SetMenuParameters(OverlayMenuParameters* menuParams)
{
	m_menuParameters = menuParams;
}

void OverlayParameters::SetInfopanelParameters(OverlayInfopanelParameters* infopanelParams)
{
	m_infopanelParameters = infopanelParams;
}

OverlayParameters* OverlayParameters::CreateDefault() noexcept
{
	const auto defaultOverlayMenu = new OverlayMenuParameters();
	const auto defaultOverlayInfopanel = new OverlayInfopanelParameters();
	return new OverlayParameters(defaultOverlayMenu, defaultOverlayInfopanel);
}

#pragma endregion

#pragma region OverlayData impl
const OverlayParameters* OverlayData::GetOverlayParameters() const noexcept
{
	return m_overlayParameters;
}
OverlayMenuParameters* OverlayData::Menu() const noexcept { return m_overlayParameters->GetMenuParameters(); }
OverlayInfopanelParameters* OverlayData::Infopanel() const noexcept { return m_overlayParameters->GetInfopanelParameters(); }
#pragma endregion

#pragma region DXData impl
DXParameters* DXData::GetDirectXParameters() const noexcept
{
	return m_dx_parameters;
}

DXFunctions* DXData::GetDirectXFunctions() const noexcept
{
	return m_dx_functions;
}
#pragma endregion

#pragma region UIData impl
const std::vector<std::string>& UIData::GetSkins() const noexcept
{
	return m_skins;
}

const std::string& UIData::GetInfoPanelMessage() const noexcept
{
	return m_displayInfo;
}

LPCSTR UIData::GetInfoPanelMessageCStr() const noexcept
{
	return m_displayInfo.c_str();
}


void UIData::AppendSkin(const std::string& skin_name)
{
	m_skins.push_back(skin_name);
}

void UIData::EraseSkin(const std::string& to_delete)
{
	auto pos = std::find(m_skins.begin(), m_skins.end(), to_delete);
	if (pos != m_skins.end())
		m_skins.erase(pos);
}

void UIData::SetSkins(const std::vector<std::string>& skins)
{
	m_skins.clear();
	m_skins.reserve(skins.size());
	m_skins.insert(m_skins.begin(), skins.begin(), skins.end());
}

void UIData::SetInfoPanelMessage(const std::string& message)
{
	m_displayInfo.clear();
	m_displayInfo.assign(message);
}





