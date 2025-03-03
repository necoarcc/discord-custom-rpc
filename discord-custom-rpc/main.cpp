#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Shlwapi.h>
#include <discord-game-sdk/discord.h>
#include <nlohmann/json.hpp>
#include <thread>
#pragma comment(lib, "shlwapi.lib")

using Json = nlohmann::json;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	WCHAR Path[MAX_PATH];
	DWORD Len = GetModuleFileNameW(NULL, Path, MAX_PATH);
	PathRemoveFileSpec(Path);
	PathAppend(Path, L"config.json");

	HANDLE File = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (File == INVALID_HANDLE_VALUE)
	{
		return 1;
	}
	DWORD FileSize = GetFileSize(File, nullptr);
	if (FileSize == INVALID_FILE_SIZE)
	{
		CloseHandle(File);
		return 1;
	}
	char* Data = new char[FileSize];
	if (!Data)
	{
		CloseHandle(File);
		return 1;
	}
	if (!ReadFile(File, Data, FileSize, nullptr, nullptr))
	{
		CloseHandle(File);
		delete[] Data;
		return 1;
	}
	CloseHandle(File);

	bool Exit = false;

	discord::Core* Core = nullptr;
	discord::Activity Activity{};
	try
	{
		Json Config = Json::parse(Data, Data + FileSize);
		delete[] Data;
		if (discord::Core::Create(Config["ClientID"].get<int64_t>(), DiscordCreateFlags_Default, &Core) != discord::Result::Ok)
			throw std::runtime_error("Failed to create a discord instance");
		auto& ActivityManager = Core->ActivityManager();
		Activity.SetDetails(Config["Details"].get<std::string>().c_str());
		Activity.SetInstance(Config["Instance"].get<int>());
		Activity.SetState(Config["State"].get<std::string>().c_str());
		auto& Assets = Activity.GetAssets();
		Assets.SetLargeImage(Config["LargeImage"].get<std::string>().c_str());
		Assets.SetLargeText(Config["LargeImageText"].get<std::string>().c_str());
		Assets.SetSmallImage(Config["SmallImage"].get<std::string>().c_str());
		Assets.SetSmallText(Config["SmallImageText"].get<std::string>().c_str());
		auto& Secrets = Activity.GetSecrets();
		Secrets.SetJoin(Config["JoinSecret"].get<std::string>().c_str());
		Secrets.SetMatch(Config["MatchSecret"].get<std::string>().c_str());
		Secrets.SetSpectate(Config["SpectateSecret"].get<std::string>().c_str());
		auto& Party = Activity.GetParty();
		Party.SetId(Config["PartyID"].get<std::string>().c_str());
		Party.SetPrivacy((discord::ActivityPartyPrivacy)Config["PartyPrivacy"].get<int>());
		ActivityManager.UpdateActivity(Activity, [&Exit](discord::Result Res)
			{
				if (Res != discord::Result::Ok)
					Exit = true;
			});;
	}
	catch (const Json::exception& e)
	{
		MessageBoxA(NULL, e.what(), "Error", MB_ICONERROR);
		delete[] Data;
		return 1;
	}
	catch (const std::runtime_error& e)
	{
		MessageBoxA(NULL, e.what(), "Error", MB_ICONERROR);
		if (Core) delete Core;
		return 1;
	}

	if (!RegisterHotKey(NULL, 1000, MOD_ALT | MOD_CONTROL, 0x41))
	{
		delete Core;
		MessageBoxW(NULL, L"Failed to register exit hotkey", L"Error", MB_ICONERROR);
		return 1;
	}

	MSG Msg{};

	MessageBoxW(NULL, L"Use CTRL+ALT+A to exit discord-custom-rpc", L"Info", MB_ICONINFORMATION);

	while (Msg.message != WM_QUIT)
	{
		if (Exit) PostQuitMessage(0);

		if (PeekMessage(&Msg, NULL, 0U, 0U, PM_REMOVE))
		{
			if (Msg.message == WM_QUIT) break;
			if (Msg.message == WM_HOTKEY && LOWORD(Msg.wParam) == 1000)
			{
				Exit = true;
			}
		}

		Core->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	UnregisterHotKey(NULL, 1000);
	delete Core;
	return Msg.wParam;
}