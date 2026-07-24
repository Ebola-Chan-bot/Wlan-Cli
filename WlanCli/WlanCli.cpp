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
#include "resource.h"
import std;

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "eappcfg.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "msxml6.lib")

std::wstring 替换全部(const std::wstring& 原文, const std::wstring& 旧片段, const std::wstring& 新片段) {
	std::wstring 结果 = 原文;
	size_t 位置 = 0;
	while ((位置 = 结果.find(旧片段, 位置)) != std::wstring::npos) {
		结果.replace(位置, 旧片段.length(), 新片段);
		位置 += 新片段.length();
	}
	return 结果;
}

std::wstring 网络标识转十六进制(const std::wstring& 网络标识) {
	std::wstring 十六进制;
	for (size_t 下标 = 0; 下标 < 网络标识.length(); 下标++) {
		wchar_t 缓冲区[3];
		swprintf_s(缓冲区, L"%02X", static_cast<unsigned char>(网络标识[下标]));
		十六进制 += 缓冲区;
	}
	return 十六进制;
}

std::wstring 全局唯一标识转字符串(const GUID& 标识) {
	wchar_t 缓冲区[39];
	swprintf_s(缓冲区, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		标识.Data1, 标识.Data2, 标识.Data3,
		标识.Data4[0], 标识.Data4[1], 标识.Data4[2], 标识.Data4[3],
		标识.Data4[4], 标识.Data4[5], 标识.Data4[6], 标识.Data4[7]);
	return 缓冲区;
}

void 输出无线错误(const wchar_t* 上下文, DWORD 错误码) {
	wprintf(L"  %s：错误 %lu (0x%08lX)\n", 上下文, 错误码, 错误码);
	switch (错误码) {
	case 87: wprintf(L"    = 参数无效 (ERROR_INVALID_PARAMETER)\n"); break;
	case 2: wprintf(L"    = 未找到文件 (ERROR_FILE_NOT_FOUND)\n"); break;
	case 5: wprintf(L"    = 拒绝访问 (ERROR_ACCESS_DENIED)\n"); break;
	case 50: wprintf(L"    = 不受支持 (ERROR_NOT_SUPPORTED)\n"); break;
	case 1168: wprintf(L"    = 未找到对象 (ERROR_NOT_FOUND)\n"); break;
	case 0x0000139F: wprintf(L"    = 状态无效 (ERROR_INVALID_STATE)\n"); break;
	}
}

bool 从资源加载宽文本(int 资源编号, std::wstring& 输出文本) {
	HRSRC 资源信息 = FindResourceW(NULL, MAKEINTRESOURCEW(资源编号), RT_RCDATA);
	if (资源信息 == NULL) {
		wprintf(L"  FindResourceW 失败：%lu\n", GetLastError());
		return false;
	}

	HGLOBAL 已加载资源 = LoadResource(NULL, 资源信息);
	if (已加载资源 == NULL) {
		wprintf(L"  LoadResource 失败：%lu\n", GetLastError());
		return false;
	}

	DWORD 资源大小 = SizeofResource(NULL, 资源信息);
	const char* 资源数据 = static_cast<const char*>(LockResource(已加载资源));
	if (资源数据 == NULL || 资源大小 == 0) {
		wprintf(L"  LockResource 失败。\n");
		return false;
	}

	int 宽字符数 = MultiByteToWideChar(CP_UTF8, 0, 资源数据, static_cast<int>(资源大小), NULL, 0);
	if (宽字符数 <= 0) {
		wprintf(L"  MultiByteToWideChar 失败：%lu\n", GetLastError());
		return false;
	}

	输出文本.assign(宽字符数, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, 资源数据, static_cast<int>(资源大小), 输出文本.data(), 宽字符数);
	return true;
}

bool 本地计算机加密(const BYTE* 原始数据, DWORD 数据长度, std::vector<BYTE>& 输出数据) {
	DATA_BLOB 输入数据块 = { 数据长度, const_cast<BYTE*>(原始数据) };
	DATA_BLOB 输出数据块 = { 0, NULL };
	if (!CryptProtectData(&输入数据块, NULL, NULL, NULL, NULL, CRYPTPROTECT_LOCAL_MACHINE, &输出数据块)) {
		wprintf(L"  CryptProtectData(LOCAL_MACHINE) 失败：%lu\n", GetLastError());
		return false;
	}

	输出数据.assign(输出数据块.pbData, 输出数据块.pbData + 输出数据块.cbData);
	LocalFree(输出数据块.pbData);
	return true;
}

bool 写入编码文本文件(const std::wstring& 文件路径, const std::wstring& 文本) {
	HANDLE 文件句柄 = CreateFileW(文件路径.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (文件句柄 == INVALID_HANDLE_VALUE) {
		return false;
	}

	int 编码后长度 = WideCharToMultiByte(CP_UTF8, 0, 文本.c_str(), -1, NULL, 0, NULL, NULL);
	if (编码后长度 <= 0) {
		CloseHandle(文件句柄);
		return false;
	}

	std::vector<char> 编码后缓冲区(编码后长度);
	WideCharToMultiByte(CP_UTF8, 0, 文本.c_str(), -1, 编码后缓冲区.data(), 编码后长度, NULL, NULL);

	DWORD 已写入字节数 = 0;
	BOOL 写入成功 = WriteFile(文件句柄, 编码后缓冲区.data(), static_cast<DWORD>(编码后长度 - 1), &已写入字节数, NULL);
	CloseHandle(文件句柄);
	return 写入成功 == TRUE;
}

bool 生成身份验证凭据数据块(const std::wstring& 用户名, const std::wstring& 密码, std::vector<BYTE>& 用户数据块, EAP_METHOD_TYPE& 输出身份验证方法) {
	std::wstring 身份验证配置文本;
	if (!从资源加载宽文本(资源标识_身份验证配置, 身份验证配置文本)) {
		wprintf(L"  错误：加载 EAP XML 资源失败。\n");
		return false;
	}

	IXMLDOMDocument2* 配置文档 = NULL;
	HRESULT 结果码 = CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER, __uuidof(IXMLDOMDocument2), reinterpret_cast<void**>(&配置文档));
	if (FAILED(结果码)) {
		wprintf(L"  CoCreateInstance(DOMDocument60) 失败：0x%08lX\n", 结果码);
		return false;
	}

	VARIANT_BOOL 已载入 = VARIANT_FALSE;
	BSTR 配置文本 = SysAllocStringLen(身份验证配置文本.c_str(), static_cast<UINT>(身份验证配置文本.size()));
	if (配置文本 == NULL) {
		wprintf(L"  SysAllocStringLen 失败。\n");
		配置文档->Release();
		return false;
	}

	结果码 = 配置文档->loadXML(配置文本, &已载入);
	SysFreeString(配置文本);
	if (FAILED(结果码) || 已载入 != VARIANT_TRUE) {
		wprintf(L"  loadXML 失败。\n");
		配置文档->Release();
		return false;
	}

	DWORD 配置数据块大小 = 0;
	BYTE* 配置数据块 = NULL;
	EAP_METHOD_TYPE 身份验证方法 = {};
	EAP_ERROR* 身份验证错误 = NULL;

	DWORD 错误码 = EapHostPeerConfigXml2Blob(0, 配置文档, &配置数据块大小, &配置数据块, &身份验证方法, &身份验证错误);
	配置文档->Release();
	if (错误码 != 0) {
		wprintf(L"  EapHostPeerConfigXml2Blob 失败：%lu\n", 错误码);
		return false;
	}
	wprintf(L"  配置数据块大小：%lu 字节\n", 配置数据块大小);

	EAP_CONFIG_INPUT_FIELD_ARRAY 输入字段数组 = {};
	错误码 = EapHostPeerQueryCredentialInputFields(NULL, 身份验证方法, 0, 配置数据块大小, 配置数据块, &输入字段数组, &身份验证错误);
	if (错误码 != 0) {
		wprintf(L"  EapHostPeerQueryCredentialInputFields 失败：%lu\n", 错误码);
		EapHostPeerFreeMemory(配置数据块);
		return false;
	}
	wprintf(L"  凭据字段数：%lu\n", 输入字段数组.dwNumberOfFields);

	if (输入字段数组.dwNumberOfFields < 2) {
		wprintf(L"  至少需要 2 个字段（用户名和密码），当前仅有 %lu 个\n", 输入字段数组.dwNumberOfFields);
		EapHostPeerFreeMemory(reinterpret_cast<BYTE*>(输入字段数组.pFields));
		EapHostPeerFreeMemory(配置数据块);
		return false;
	}

	输入字段数组.pFields[0].pwszData = _wcsdup(用户名.c_str());
	输入字段数组.pFields[1].pwszData = _wcsdup(密码.c_str());
	wprintf(L"  已设置用户名和密码\n");

	DWORD 用户数据块大小 = 0;
	BYTE* 用户数据块指针 = NULL;
	错误码 = EapHostPeerQueryUserBlobFromCredentialInputFields(NULL, 身份验证方法, 0, 配置数据块大小, 配置数据块, &输入字段数组, &用户数据块大小, &用户数据块指针, &身份验证错误);
	if (错误码 != 0) {
		wprintf(L"  EapHostPeerQueryUserBlobFromCredentialInputFields 失败：%lu\n", 错误码);
		free(输入字段数组.pFields[0].pwszData);
		free(输入字段数组.pFields[1].pwszData);
		EapHostPeerFreeMemory(reinterpret_cast<BYTE*>(输入字段数组.pFields));
		EapHostPeerFreeMemory(配置数据块);
		return false;
	}
	wprintf(L"  用户数据块大小：%lu 字节\n", 用户数据块大小);

	用户数据块.assign(用户数据块指针, 用户数据块指针 + 用户数据块大小);
	输出身份验证方法 = 身份验证方法;

	for (DWORD 下标 = 0; 下标 < 输入字段数组.dwNumberOfFields; 下标++) {
		free(输入字段数组.pFields[下标].pwszData);
	}
	EapHostPeerFreeMemory(reinterpret_cast<BYTE*>(输入字段数组.pFields));
	EapHostPeerFreeMemory(用户数据块指针);
	EapHostPeerFreeMemory(配置数据块);
	return true;
}

bool 查找配置唯一标识(const GUID& 接口唯一标识, const std::wstring& 网络标识, std::wstring& 配置唯一标识) {
	std::wstring 接口唯一标识字符串 = 全局唯一标识转字符串(接口唯一标识);

	wchar_t 搜索路径[512];
	swprintf_s(搜索路径, L"C:\\ProgramData\\Microsoft\\Wlansvc\\Profiles\\Interfaces\\%s\\*.xml", 接口唯一标识字符串.c_str());

	WIN32_FIND_DATAW 查找数据;
	HANDLE 查找句柄 = FindFirstFileW(搜索路径, &查找数据);
	if (查找句柄 == INVALID_HANDLE_VALUE) {
		return false;
	}

	do {
		wchar_t 完整路径[1024];
		swprintf_s(完整路径, L"C:\\ProgramData\\Microsoft\\Wlansvc\\Profiles\\Interfaces\\%s\\%s", 接口唯一标识字符串.c_str(), 查找数据.cFileName);

		HANDLE 文件句柄 = CreateFileW(完整路径, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (文件句柄 == INVALID_HANDLE_VALUE) {
			continue;
		}

		DWORD 文件大小 = GetFileSize(文件句柄, NULL);
		if (文件大小 > 0 && 文件大小 < 65536) {
			std::vector<char> 缓冲区(文件大小 + 2);
			DWORD 已读取字节数 = 0;
			if (ReadFile(文件句柄, 缓冲区.data(), 文件大小, &已读取字节数, NULL)) {
				缓冲区[已读取字节数] = 0;
				缓冲区[已读取字节数 + 1] = 0;
				int 宽字符数 = MultiByteToWideChar(CP_UTF8, 0, 缓冲区.data(), 已读取字节数, NULL, 0);
				std::wstring 文件内容(宽字符数, L'\0');
				MultiByteToWideChar(CP_UTF8, 0, 缓冲区.data(), 已读取字节数, 文件内容.data(), 宽字符数);
				if (文件内容.find(L"<name>" + 网络标识 + L"</name>") != std::wstring::npos) {
					std::wstring 文件名(查找数据.cFileName);
					配置唯一标识 = 文件名.substr(0, 文件名.length() - 4);
					CloseHandle(文件句柄);
					FindClose(查找句柄);
					return true;
				}
			}
		}

		CloseHandle(文件句柄);
	} while (FindNextFileW(查找句柄, &查找数据));

	FindClose(查找句柄);
	return false;
}

bool 写入配置凭据数据(const std::wstring& 配置唯一标识, const std::vector<BYTE>& 数据) {
	std::wstring 注册表路径 = L"SOFTWARE\\Microsoft\\Wlansvc\\UserData\\Profiles\\" + 配置唯一标识;

	HKEY 注册表句柄 = NULL;
	LSTATUS 状态码 = RegCreateKeyExW(HKEY_CURRENT_USER, 注册表路径.c_str(), 0, NULL, 0, KEY_SET_VALUE, NULL, &注册表句柄, NULL);
	if (状态码 != ERROR_SUCCESS) {
		wprintf(L"  RegCreateKeyEx 失败：%lu\n", 状态码);
		return false;
	}

	状态码 = RegSetValueExW(注册表句柄, L"MSMUserData", 0, REG_BINARY, 数据.data(), static_cast<DWORD>(数据.size()));
	RegCloseKey(注册表句柄);
	if (状态码 != ERROR_SUCCESS) {
		wprintf(L"  RegSetValueEx(MSMUserData) 失败：%lu\n", 状态码);
		return false;
	}

	wprintf(L"  已向 MSMUserData 写入 %zu 字节\n", 数据.size());
	return true;
}

int wmain(int 参数个数, wchar_t* 参数值[]) {
	std::wstring 网络标识;
	std::wstring 用户名;
	std::wstring 密码;
	std::wstring 接口名称;

	for (int 下标 = 1; 下标 < 参数个数; 下标++) {
		std::wstring 当前参数 = 参数值[下标];
		if (当前参数 == L"--网络标识" && 下标 + 1 < 参数个数) {
			网络标识 = 参数值[++下标];
		}
		else if (当前参数 == L"--用户名" && 下标 + 1 < 参数个数) {
			用户名 = 参数值[++下标];
		}
		else if (当前参数 == L"--密码" && 下标 + 1 < 参数个数) {
			密码 = 参数值[++下标];
		}
		else if (当前参数 == L"--接口" && 下标 + 1 < 参数个数) {
			接口名称 = 参数值[++下标];
		}
		else if (当前参数 == L"--帮助") {
			wprintf(L"用法：wififix.exe --网络标识 <SSID> --用户名 <用户名> --密码 <密码> [--接口 <接口名称>]\n");
			wprintf(L"\n用于修复 Windows 11 24H2 下 WPA2 企业级无线网络凭据问题。\n");
			return 0;
		}
	}

	if (网络标识.empty() || 用户名.empty() || 密码.empty()) {
		wprintf(L"错误：必须提供 --网络标识、--用户名 和 --密码。\n");
		wprintf(L"用法：wififix.exe --网络标识 <SSID> --用户名 <用户名> --密码 <密码> [--接口 <接口名称>]\n");
		return 1;
	}

	wprintf(L"=== WPA2 企业级无线网络修复工具 ===\n");
	wprintf(L"  网络标识：%s\n", 网络标识.c_str());
	wprintf(L"  用户名：  %s\n", 用户名.c_str());
	wprintf(L"  密码：    ********\n");
	if (!接口名称.empty()) {
		wprintf(L"  接口名称：%s\n", 接口名称.c_str());
	}

	HRESULT 初始化结果 = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	bool 已初始化组件 = SUCCEEDED(初始化结果) || 初始化结果 == RPC_E_CHANGED_MODE;
	if (!已初始化组件) {
		wprintf(L"错误：CoInitializeEx 失败：0x%08lX\n", 初始化结果);
		return 1;
	}

	HANDLE 无线句柄 = NULL;
	DWORD 协议版本 = 0;
	DWORD 错误码 = WlanOpenHandle(2, NULL, &协议版本, &无线句柄);
	if (错误码 != ERROR_SUCCESS) {
		输出无线错误(L"WlanOpenHandle", 错误码);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"\n[1] 已打开无线局域网句柄（版本 %lu）\n", 协议版本);

	PWLAN_INTERFACE_INFO_LIST 接口列表 = NULL;
	错误码 = WlanEnumInterfaces(无线句柄, NULL, &接口列表);
	if (错误码 != ERROR_SUCCESS) {
		输出无线错误(L"WlanEnumInterfaces", 错误码);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}

	wprintf(L"[2] 找到 %lu 个无线接口：\n", 接口列表->dwNumberOfItems);
	for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++) {
		auto& 接口信息 = 接口列表->InterfaceInfo[下标];
		wprintf(L"    [%lu] \"%s\"（唯一标识：%s） 状态=%lu\n", 下标, 接口信息.strInterfaceDescription, 全局唯一标识转字符串(接口信息.InterfaceGuid).c_str(), 接口信息.isState);
	}

	if (接口列表->dwNumberOfItems == 0) {
		wprintf(L"  错误：未找到无线接口！\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}

	GUID 目标唯一标识 = {};
	if (接口列表->dwNumberOfItems == 1) {
		目标唯一标识 = 接口列表->InterfaceInfo[0].InterfaceGuid;
		wprintf(L"  已自动选择唯一接口：\"%s\"\n", 接口列表->InterfaceInfo[0].strInterfaceDescription);
	}
	else if (!接口名称.empty()) {
		bool 已匹配 = false;
		for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++) {
			std::wstring 接口描述(接口列表->InterfaceInfo[下标].strInterfaceDescription);
			if (接口名称 == 接口描述 || 接口描述.find(接口名称) != std::wstring::npos) {
				目标唯一标识 = 接口列表->InterfaceInfo[下标].InterfaceGuid;
				已匹配 = true;
				wprintf(L"  已匹配接口：\"%s\"\n", 接口描述.c_str());
				break;
			}
		}
		if (!已匹配) {
			wprintf(L"  错误：没有匹配 \"%s\" 的接口。\n", 接口名称.c_str());
			wprintf(L"  可用接口：\n");
			for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++) {
				wprintf(L"    [%lu] \"%s\"\n", 下标, 接口列表->InterfaceInfo[下标].strInterfaceDescription);
			}
			WlanFreeMemory(接口列表);
			WlanCloseHandle(无线句柄, NULL);
			if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
				CoUninitialize();
			}
			return 1;
		}
	}
	else {
		wprintf(L"  错误：存在多个接口，但未指定 --接口。\n");
		wprintf(L"  可用接口：\n");
		for (DWORD 下标 = 0; 下标 < 接口列表->dwNumberOfItems; 下标++) {
			wprintf(L"    [%lu] \"%s\"\n", 下标, 接口列表->InterfaceInfo[下标].strInterfaceDescription);
		}
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}

	wprintf(L"\n[3] 正在删除现有配置文件 \"%s\"...\n", 网络标识.c_str());
	std::wstring 命令行 = L"netsh wlan delete profile name=\"" + 网络标识 + L"\" 2>nul";
	_wsystem(命令行.c_str());
	wprintf(L"  完成。\n");

	wprintf(L"\n[4] 正在通过 netsh 添加所有用户配置文件...\n");
	std::wstring 无线配置文本;
	if (!从资源加载宽文本(资源标识_无线配置, 无线配置文本)) {
		wprintf(L"  错误：加载配置 XML 资源失败。\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}

	std::wstring 十六进制网络标识 = 网络标识转十六进制(网络标识);
	无线配置文本 = 替换全部(无线配置文本, L"{ssid_name}", 网络标识);
	无线配置文本 = 替换全部(无线配置文本, L"{ssid_hex}", 十六进制网络标识);

	wchar_t 临时目录[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, 临时目录);
	std::wstring 配置文本文件 = std::wstring(临时目录) + L"wififix_profile.xml";
	if (!写入编码文本文件(配置文本文件, 无线配置文本)) {
		wprintf(L"  错误：无法创建临时 XML 文件。\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"  XML 已保存到：%s\n", 配置文本文件.c_str());

	命令行 = L"netsh wlan add profile filename=\"" + 配置文本文件 + L"\" user=all";
	int 返回码 = _wsystem(命令行.c_str());
	if (返回码 != 0) {
		wprintf(L"  错误：netsh 添加配置文件失败（返回码=%d）。\n", 返回码);
		DeleteFileW(配置文本文件.c_str());
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"  已添加配置文件（所有用户）。\n");
	DeleteFileW(配置文本文件.c_str());

	wprintf(L"\n[5] 正在生成可扩展身份验证凭据数据块...\n");
	std::vector<BYTE> 身份验证数据块;
	EAP_METHOD_TYPE 身份验证方法 = {};
	if (!生成身份验证凭据数据块(用户名, 密码, 身份验证数据块, 身份验证方法)) {
		wprintf(L"  错误：生成 EAP 数据块失败。\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}

	wprintf(L"\n[6] 正在向 WlanSvc 注册凭据...\n");
	错误码 = WlanSetProfileEapUserData(无线句柄, &目标唯一标识, 网络标识.c_str(), 身份验证方法, 1, static_cast<DWORD>(身份验证数据块.size()), 身份验证数据块.data(), NULL);
	if (错误码 != ERROR_SUCCESS) {
		输出无线错误(L"WlanSetProfileEapUserData", 错误码);
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"  已注册凭据（所有用户）。\n");

	wprintf(L"\n[7] 正在使用 LOCAL_MACHINE DPAPI 加密...\n");
	std::vector<BYTE> 加密数据;
	if (!本地计算机加密(身份验证数据块.data(), static_cast<DWORD>(身份验证数据块.size()), 加密数据)) {
		wprintf(L"  错误：DPAPI 加密失败。\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"  已加密：%zu 字节（原始：%zu 字节）\n", 加密数据.size(), 身份验证数据块.size());

	wprintf(L"\n[8] 正在用本地计算机加密数据覆写 MSMUserData...\n");
	std::wstring 配置唯一标识;
	if (!查找配置唯一标识(目标唯一标识, 网络标识, 配置唯一标识)) {
		wprintf(L"  错误：无法在 ProgramData 中找到配置唯一标识。\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"  配置唯一标识：%s\n", 配置唯一标识.c_str());

	if (!写入配置凭据数据(配置唯一标识, 加密数据)) {
		wprintf(L"  错误：写入凭据失败。\n");
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}

	wprintf(L"\n[9] 正在连接...\n");
	WLAN_CONNECTION_PARAMETERS 连接参数 = {};
	连接参数.wlanConnectionMode = wlan_connection_mode_profile;
	连接参数.strProfile = 网络标识.c_str();
	连接参数.dot11BssType = dot11_BSS_type_infrastructure;
	连接参数.dwFlags = 0;

	错误码 = WlanConnect(无线句柄, &目标唯一标识, &连接参数, NULL);
	if (错误码 != ERROR_SUCCESS) {
		输出无线错误(L"WlanConnect", 错误码);
		WlanFreeMemory(接口列表);
		WlanCloseHandle(无线句柄, NULL);
		if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
			CoUninitialize();
		}
		return 1;
	}
	wprintf(L"  已成功发起连接。\n");

	wprintf(L"  正在等待连接...\n");
	Sleep(5000);

	PWLAN_CONNECTION_ATTRIBUTES 连接属性 = NULL;
	DWORD 属性大小 = sizeof(WLAN_CONNECTION_ATTRIBUTES);
	错误码 = WlanQueryInterface(无线句柄, &目标唯一标识, wlan_intf_opcode_current_connection, NULL, &属性大小, reinterpret_cast<PVOID*>(&连接属性), NULL);
	if (错误码 == ERROR_SUCCESS && 连接属性 != NULL) {
		if (连接属性->isState == wlan_interface_state_connected) {
			wprintf(L"\n  *** 已连接！***\n");
			wprintf(L"  网络标识：%.*hs\n", static_cast<int>(连接属性->wlanAssociationAttributes.dot11Ssid.uSSIDLength), 连接属性->wlanAssociationAttributes.dot11Ssid.ucSSID);
			wprintf(L"  接入点地址：%02X:%02X:%02X:%02X:%02X:%02X\n", 连接属性->wlanAssociationAttributes.dot11Bssid[0], 连接属性->wlanAssociationAttributes.dot11Bssid[1], 连接属性->wlanAssociationAttributes.dot11Bssid[2], 连接属性->wlanAssociationAttributes.dot11Bssid[3], 连接属性->wlanAssociationAttributes.dot11Bssid[4], 连接属性->wlanAssociationAttributes.dot11Bssid[5]);
			wprintf(L"  认证：%lu，加密：%lu，信号：%lu%%\n", 连接属性->wlanSecurityAttributes.bSecurityEnabled ? 连接属性->wlanSecurityAttributes.dot11AuthAlgorithm : 0, 连接属性->wlanSecurityAttributes.dot11CipherAlgorithm, 连接属性->wlanAssociationAttributes.wlanSignalQuality);
		}
		else {
			wprintf(L"\n  状态：%lu（仍在连接中或连接失败）\n", 连接属性->isState);
		}
		WlanFreeMemory(连接属性);
	}

	WlanFreeMemory(接口列表);
	WlanCloseHandle(无线句柄, NULL);
	if (初始化结果 == S_OK || 初始化结果 == S_FALSE) {
		CoUninitialize();
	}

	wprintf(L"\n完成。\n");
	return 0;
}
