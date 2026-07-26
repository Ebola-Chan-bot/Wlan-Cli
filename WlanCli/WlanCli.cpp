#include <windows.h>
#include <wlanapi.h>
#include <dpapi.h>
#include <eaphostpeerconfigapis.h>
#include <eaptypes.h>
#include <winreg.h>
#include <shellapi.h>
#include <msxml6.h>
#include <stdio.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include "resource.h"
#ifdef __INTELLISENSE__
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <filesystem>
#else
import std;
#endif

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "eappcfg.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "msxml6.lib")
#pragma comment(lib, "rpcrt4.lib")

static void 替换全部(std::wstring& 原文, wchar_t const* 旧片段, wchar_t const* 新片段) {
	size_t 位置 = 0;
	size_t const 旧片段长度 = wcslen(旧片段);
	size_t const 新片段长度 = wcslen(新片段);
	while ((位置 = 原文.find(旧片段, 位置)) != std::wstring::npos) {
		原文.replace(位置, 旧片段长度, 新片段);
		位置 += 新片段长度;
	}
}

static void 替换全部(std::string& 原文, char const* 旧片段, char const* 新片段) {
	size_t 位置 = 0;
	size_t const 旧片段长度 = strlen(旧片段);
	size_t const 新片段长度 = strlen(新片段);
	while ((位置 = 原文.find(旧片段, 位置)) != std::string::npos) {
		原文.replace(位置, 旧片段长度, 新片段);
		位置 += 新片段长度;
	}
}

static void 输出无线错误(const wchar_t* 上下文, DWORD 错误码) {
	wprintf(L"  %s：错误 %lu (0x%08lX)\n", 上下文, 错误码, 错误码);

	wchar_t* 消息缓冲区 = nullptr;
	DWORD 消息长度 = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, 错误码, 0, reinterpret_cast<LPWSTR>(&消息缓冲区), 0, nullptr);
	if (消息缓冲区) {
		wprintf(L"    = %s\n", 消息缓冲区);
		LocalFree(消息缓冲区);
	}
}

static void const* 从资源加载字节(int 资源编号, DWORD& 输出大小) {
	HRSRC const 资源信息 = FindResourceW(NULL, MAKEINTRESOURCEW(资源编号), RT_RCDATA);
	输出大小 = SizeofResource(NULL, 资源信息);
	return LockResource(LoadResource(NULL, 资源信息));
}

int wmain(int 参数个数, wchar_t* 参数值[]) {
	_setmode(_fileno(stdout), _O_U16TEXT);

	LPWSTR 网络标识 = nullptr;
	LPWSTR 用户名 = nullptr;
	LPWSTR 密码 = nullptr;
	LPWSTR 接口名称 = nullptr;

	for (int 下标 = 1; 下标 < 参数个数; 下标++) {
		wchar_t const* 当前参数 = 参数值[下标];
		if (!wcscmp(当前参数, L"-网络标识") && 下标 + 1 < 参数个数) {
			网络标识 = 参数值[++下标];
		}
		else if (!wcscmp(当前参数, L"-用户名") && 下标 + 1 < 参数个数) {
			用户名 = 参数值[++下标];
		}
		else if (!wcscmp(当前参数, L"-密码") && 下标 + 1 < 参数个数) {
			密码 = 参数值[++下标];
		}
		else if (!wcscmp(当前参数, L"-接口") && 下标 + 1 < 参数个数) {
			接口名称 = 参数值[++下标];
		}
		else if (!wcscmp(当前参数, L"-帮助")) {
			wprintf(L"用法：WlanCli -网络标识 <SSID> -用户名 <用户名> -密码 <密码> [-接口 <接口名称>]\n");
			wprintf(L"\n用于修复 Windows 11 24H2 下 WPA2 企业级无线网络凭据问题。\n");
			return 0;
		}
	}

	if (!(网络标识 && 用户名 && 密码)) {
		wprintf(L"错误：必须提供 -网络标识、-用户名 和 -密码。\n");
		wprintf(L"用法：WlanCli -网络标识 <SSID> -用户名 <用户名> -密码 <密码> [-接口 <接口名称>]\n");
		return 1;
	}

	wprintf(L"=== WPA2 企业级无线网络修复工具 ===\n");
	wprintf(L"  网络标识：%s\n", 网络标识);
	wprintf(L"  用户名：  %s\n", 用户名);
	wprintf(L"  密码：    ********\n");
	if (接口名称)
		wprintf(L"  接口名称：%s\n", 接口名称);

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	HANDLE 无线句柄;
	DWORD 尺寸;
	WlanOpenHandle(2, NULL, &尺寸, &无线句柄);
	wprintf(L"\n[1] 已打开无线局域网句柄（版本 %lu）\n", 尺寸);

	PWLAN_INTERFACE_INFO_LIST 接口列表;
	WlanEnumInterfaces(无线句柄, NULL, &接口列表);


	wprintf(L"[2] 找到 %lu 个无线接口：\n", 接口列表->dwNumberOfItems);
	RPC_WSTR 目标唯一标识字符串;
	for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++) {
		auto& 接口信息 = 接口列表->InterfaceInfo[下标];
		UuidToStringW(&接口信息.InterfaceGuid, &目标唯一标识字符串);
		wprintf(L"    [%lu] \"%s\"（唯一标识：%s） 状态=%lu\n", 下标, 接口信息.strInterfaceDescription, reinterpret_cast<wchar_t const*>(目标唯一标识字符串), 接口信息.isState);
	}

	if (!接口列表->dwNumberOfItems) {
		wprintf(L"  错误：未找到无线接口！\n");
		return 1;
	}

	GUID 目标唯一标识;
	if (接口名称) {
		bool 已匹配 = false;
		for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++) {
			std::wstring 接口描述(接口列表->InterfaceInfo[下标].strInterfaceDescription);
			if (!wcscmp(接口名称, 接口描述.c_str()) || 接口描述.find(接口名称) != std::wstring::npos) {
				目标唯一标识 = 接口列表->InterfaceInfo[下标].InterfaceGuid;
				已匹配 = true;
				wprintf(L"  已匹配接口：\"%s\"\n", 接口描述.c_str());
				break;
			}
		}
		if (!已匹配) {
			wprintf(L"  错误：没有匹配 \"%s\" 的接口。\n", 接口名称);
			wprintf(L"  可用接口：\n");
			for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++)
				wprintf(L"    [%lu] \"%s\"\n", 下标, 接口列表->InterfaceInfo[下标].strInterfaceDescription);
			return 1;
		}
	}
	else if (接口列表->dwNumberOfItems == 1) {
		目标唯一标识 = 接口列表->InterfaceInfo[0].InterfaceGuid;
		wprintf(L"  已自动选择唯一接口：\"%s\"\n", 接口列表->InterfaceInfo[0].strInterfaceDescription);
	}
	else {
		wprintf(L"  错误：存在多个接口，但未指定 -接口。\n");
		wprintf(L"  可用接口：\n");
		for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++)
			wprintf(L"    [%lu] \"%s\"\n", 下标, 接口列表->InterfaceInfo[下标].strInterfaceDescription);
		return 1;
	}

	wprintf(L"\n[3] 正在删除现有配置文件 \"%s\"...\n", 网络标识);
	_wsystem((L"netsh wlan delete profile name=\"" + std::wstring(网络标识) + L"\" 2>nul").c_str());
	wprintf(L"  完成。\n");

	wprintf(L"\n[4] 正在通过 netsh 添加所有用户配置文件...\n");

	// 加载 UTF-8 模板
	auto const* const 模板数据 = static_cast<char const*>(从资源加载字节(资源标识_无线配置, 尺寸));
	std::string 临时文本(模板数据, 尺寸);

	// SSID → UTF-8
	std::string 网络标识_utf8;
	网络标识_utf8.resize_and_overwrite(
		WideCharToMultiByte(CP_UTF8, 0, 网络标识, -1, nullptr, 0, nullptr, nullptr) - 1,
		[&](char* 缓冲区, size_t 长度) {
			WideCharToMultiByte(CP_UTF8, 0, 网络标识, -1, 缓冲区, static_cast<int>(长度) + 1, nullptr, nullptr);
			return 长度;
		});

	// SSID hex（UTF-8 字节 → 十六进制）
	std::ostringstream 十六进制网络标识;
	十六进制网络标识 << std::uppercase << std::hex << std::setfill('0');
	for (unsigned char 字节 : 网络标识_utf8)
		十六进制网络标识 << std::setw(2) << static_cast<int>(字节);

	// 替换模板占位符（纯 ASCII 操作）
	替换全部(临时文本, "{ssid_name}", 网络标识_utf8.c_str());
	替换全部(临时文本, "{ssid_hex}", 十六进制网络标识.str().c_str());

	// 临时文件路径
	std::wstring 路径文本;
	路径文本.resize_and_overwrite(
		GetTempPath2W(0, nullptr) + _countof(L"wififix_profile.xml") - 1,
		[](wchar_t* 缓冲区, size_t 长度) {
			auto const 前缀长度 = GetTempPath2W(static_cast<DWORD>(长度), 缓冲区);
			wcscpy_s(缓冲区 + 前缀长度 - 1, 长度 - 前缀长度 + 1, L"wififix_profile.xml");
			return 前缀长度 - 1 + _countof(L"wififix_profile.xml") - 1;
		});

	// 直接写 UTF-8 字节，无需转码
	HANDLE 文件句柄 = CreateFileW(路径文本.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	WriteFile(文件句柄, 临时文本.data(), static_cast<DWORD>(临时文本.size()), nullptr, nullptr);
	CloseHandle(文件句柄);
	wprintf(L"  XML 已保存到：%s\n", 路径文本.c_str());

	_wsystem((L"netsh wlan add profile filename=\"" + 路径文本 + L"\" user=all").c_str());
	wprintf(L"  已添加配置文件（所有用户）。\n");

	wprintf(L"\n[5] 正在生成可扩展身份验证凭据数据块...\n");
	EAP_METHOD_TYPE 身份验证方法;
	auto const* const 配置文本数据 = static_cast<wchar_t const*>(从资源加载字节(资源标识_身份验证配置, 尺寸));
	尺寸 = 尺寸 / sizeof(wchar_t);

	IXMLDOMDocument2* 配置文档;
	CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER, __uuidof(IXMLDOMDocument2), reinterpret_cast<void**>(&配置文档));

	VARIANT_BOOL 已载入;
	BSTR 配置文本 = SysAllocStringLen(配置文本数据, 尺寸);

	配置文档->loadXML(配置文本, &已载入);

	BYTE* 配置数据块;
	EAP_ERROR* 身份验证错误;
	DWORD 配置数据块尺寸;

	EapHostPeerConfigXml2Blob(0, 配置文档, &配置数据块尺寸, &配置数据块, &身份验证方法, &身份验证错误);
	wprintf(L"  配置数据块大小：%lu 字节\n", 配置数据块尺寸);

	EAP_CONFIG_INPUT_FIELD_ARRAY 输入字段数组;
	EapHostPeerQueryCredentialInputFields(NULL, 身份验证方法, 0, 配置数据块尺寸, 配置数据块, &输入字段数组, &身份验证错误);
	wprintf(L"  凭据字段数：%lu\n", 输入字段数组.dwNumberOfFields);

	输入字段数组.pFields[0].pwszData = 用户名;
	输入字段数组.pFields[1].pwszData = 密码;
	wprintf(L"  已设置用户名和密码\n");

	BYTE* 用户数据块 = nullptr;
	尺寸 = 0;
	//EapHostPeerQueryUserBlobFromCredentialInputFields会检查这两个参数，不设为0时行为不同
	EapHostPeerQueryUserBlobFromCredentialInputFields(NULL, 身份验证方法, 0, 配置数据块尺寸, 配置数据块, &输入字段数组, &尺寸, &用户数据块, &身份验证错误);

	wprintf(L"  用户数据块大小：%lu 字节\n", 尺寸);

	wprintf(L"\n[6] 正在向 WlanSvc 注册凭据...\n");
	WlanSetProfileEapUserData(无线句柄, &目标唯一标识, 网络标识, 身份验证方法, 1, 尺寸, 用户数据块, NULL);
	wprintf(L"  已注册凭据（所有用户）。\n");

	wprintf(L"\n[7] 正在使用 LOCAL_MACHINE DPAPI 加密...\n");
	DATA_BLOB 输入数据块 = { 尺寸, const_cast<BYTE*>(用户数据块) };
	DATA_BLOB 加密数据;
	CryptProtectData(&输入数据块, NULL, NULL, NULL, NULL, CRYPTPROTECT_LOCAL_MACHINE, &加密数据);

	wprintf(L"  已加密：%u 字节（原始：%u 字节）\n", 加密数据.cbData, 尺寸);

	wprintf(L"\n[8] 正在用本地计算机加密数据覆写 MSMUserData...\n");
	UuidToStringW(&目标唯一标识, &目标唯一标识字符串);

	路径文本.resize_and_overwrite(
		GetEnvironmentVariableW(L"ProgramData", nullptr, 0),
		[](wchar_t* 缓冲区, size_t 长度) {
			GetEnvironmentVariableW(L"ProgramData", 缓冲区, static_cast<DWORD>(长度));
			return 长度 - 1;
		});

	// 用步骤 [4] 已转好的 UTF-8 SSID 构造搜索模式，直接在原始字节中匹配
	临时文本 = "<name>" + 网络标识_utf8 + "</name>";

	for (auto const& 条目 : std::filesystem::directory_iterator{ std::filesystem::path{路径文本 + L"\\Microsoft\\Wlansvc\\Profiles\\Interfaces\\{" + reinterpret_cast<wchar_t const*>(目标唯一标识字符串) + L"}"} }) {
		if (!条目.is_regular_file() || 条目.path().extension() != L".xml") continue;

		文件句柄 = CreateFileW(条目.path().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (文件句柄 == INVALID_HANDLE_VALUE) continue;

		尺寸 = GetFileSize(文件句柄, nullptr);
		HANDLE const 映射句柄 = CreateFileMappingW(文件句柄, nullptr, PAGE_READONLY, 0, 0, nullptr);
		auto const* const 映射数据 = static_cast<char const*>(MapViewOfFile(映射句柄, FILE_MAP_READ, 0, 0, 0));
		size_t const 搜索结果 = std::string_view(映射数据, 尺寸).find(临时文本);
		UnmapViewOfFile(映射数据);
		CloseHandle(映射句柄);
		CloseHandle(文件句柄);

		if (搜索结果 != std::string_view::npos) {
			路径文本 = 条目.path().stem().wstring();
			break;
		}
	}
	wprintf(L"  配置唯一标识：%s\n", 路径文本.c_str());

	路径文本 = L"SOFTWARE\\Microsoft\\Wlansvc\\UserData\\Profiles\\" + 路径文本;

	HKEY 注册表句柄;
	RegCreateKeyExW(HKEY_CURRENT_USER, 路径文本.c_str(), 0, NULL, 0, KEY_SET_VALUE, NULL, &注册表句柄, NULL);

	RegSetValueExW(注册表句柄, L"MSMUserData", 0, REG_BINARY, 加密数据.pbData, 加密数据.cbData);
	RegCloseKey(注册表句柄);

	wprintf(L"  已向 MSMUserData 写入 %lu 字节\n", 加密数据.cbData);

	wprintf(L"\n[9] 正在连接...\n");

	WLAN_CONNECTION_PARAMETERS 连接参数{ .wlanConnectionMode = wlan_connection_mode_profile,.strProfile = 网络标识,.dot11BssType = dot11_BSS_type_infrastructure,.dwFlags = 0 };
	//WlanConnect要求WLAN_CONNECTION_PARAMETERS其它成员必须为NULL，不能为未定义值
	WlanConnect(无线句柄, &目标唯一标识, &连接参数, NULL);

	wprintf(L"  已成功发起连接。\n");

	wprintf(L"  正在等待连接...\n");
	文件句柄 = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	DWORD 之前通知源 = 0;
	struct 通知上下文
	{
		HANDLE 事件;
		GUID const* 目标;
	} 上下文 = { 文件句柄, &目标唯一标识 };
	WlanRegisterNotification(无线句柄, WLAN_NOTIFICATION_SOURCE_ACM, FALSE,
		[](PWLAN_NOTIFICATION_DATA 通知, PVOID 上下文指针) {
			auto& ctx = *reinterpret_cast<通知上下文*>(上下文指针);
			if (通知->NotificationCode == wlan_notification_acm_connection_complete &&
				IsEqualGUID(通知->InterfaceGuid, *ctx.目标))
				SetEvent(ctx.事件);
		},
		&上下文, nullptr, &之前通知源);

	if (WaitForSingleObject(文件句柄, INFINITE) == WAIT_OBJECT_0) {
		PWLAN_CONNECTION_ATTRIBUTES 连接属性;
		DWORD 属性大小 = sizeof(WLAN_CONNECTION_ATTRIBUTES);
		WlanQueryInterface(无线句柄, &目标唯一标识, wlan_intf_opcode_current_connection,
			nullptr, &属性大小, reinterpret_cast<PVOID*>(&连接属性), nullptr);
		if (连接属性->isState == wlan_interface_state_connected) {
			wprintf(L"\n  *** 已连接！***\n");
			wprintf(L"  网络标识：%.*hs\n", static_cast<int>(连接属性->wlanAssociationAttributes.dot11Ssid.uSSIDLength), 连接属性->wlanAssociationAttributes.dot11Ssid.ucSSID);
			wprintf(L"  接入点地址：%02X:%02X:%02X:%02X:%02X:%02X\n", 连接属性->wlanAssociationAttributes.dot11Bssid[0], 连接属性->wlanAssociationAttributes.dot11Bssid[1], 连接属性->wlanAssociationAttributes.dot11Bssid[2], 连接属性->wlanAssociationAttributes.dot11Bssid[3], 连接属性->wlanAssociationAttributes.dot11Bssid[4], 连接属性->wlanAssociationAttributes.dot11Bssid[5]);
			wprintf(L"  认证：%lu，加密：%lu，信号：%lu%%\n", 连接属性->wlanSecurityAttributes.bSecurityEnabled ? 连接属性->wlanSecurityAttributes.dot11AuthAlgorithm : 0, 连接属性->wlanSecurityAttributes.dot11CipherAlgorithm, 连接属性->wlanAssociationAttributes.wlanSignalQuality);
		} else {
			wprintf(L"\n  连接未成功，接口状态=%lu\n", 连接属性->isState);
		}
		WlanFreeMemory(连接属性);
	}

	wprintf(L"\n完成。\n");
	return 0;
}
