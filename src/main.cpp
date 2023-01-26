#include <GarrysMod/InterfacePointers.hpp>
#include <GarrysMod/FunctionPointers.hpp>
#include <GarrysMod/FactoryLoader.hpp>
#include <GarrysMod/ModuleLoader.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <eiface.h>
#include <interface.h>
#include <detouring/hook.hpp>
#include <chrono>
#include <map>
#include <inetchannel.h>

// Define CNetChan in terms of INetChannel
class CNetChan : public INetChannel
{
};

namespace global
{
	// Create a global lua state
	GarrysMod::Lua::ILuaInterface *lua = nullptr;
	
	// Create a global pointer to the original function
	FunctionPointers::CNetChan_ProcessMessages_t ProcessMessages_original = nullptr;

	// Create a pair of a uint64 and a chrono::duration
	using TimePair = std::pair<double, std::chrono::duration<int64_t, std::nano>>;
	Detouring::Hook ProcessMessagesHook;
	std::map<CNetChan *, TimePair> ProcessingTimes;

	bool ProcessMessages_Hook(CNetChan *Channel, bf_read &Buffer)
	{
		// Get the original function
		static FunctionPointers::CNetChan_ProcessMessages_t Trampoline = ProcessMessagesHook.GetTrampoline<FunctionPointers::CNetChan_ProcessMessages_t>();
		static ConVar *net_chan_limit_msec;

		// Get the net_chan_limit_msec convar
		if (!net_chan_limit_msec)
			net_chan_limit_msec = InterfacePointers::Cvar()->FindVar("net_chan_limit_msec");

		// If the convar is not set or is set to 0, call the original function
		if (!net_chan_limit_msec || net_chan_limit_msec->GetInt() == 0)
			return Trampoline(Channel, Buffer);

		// Get the processing time for the client and call the original function
		std::chrono::time_point Start = std::chrono::system_clock::now();
		bool Return = Trampoline(Channel, Buffer);
		std::chrono::time_point End = std::chrono::system_clock::now();
		const double MS = ((End.time_since_epoch() - Start.time_since_epoch()) / 1000.0f / 1000.0f).count();

		// Create a new entry if the client is not in the map
		if (ProcessingTimes.find(Channel) == ProcessingTimes.end())
			ProcessingTimes[Channel] = std::make_pair<double, std::chrono::duration<int64_t, std::nano>>(0, std::chrono::system_clock::time_point::duration(0));

		// Reset the processing time if it has been more than a milisecond since the last reset
		TimePair &Data = ProcessingTimes[Channel];

		// Check if the time has been more than a milisecond since the last reset
		if (Data.second + std::chrono::seconds(1) < End.time_since_epoch())
		{
			Data.first = 0;
			Data.second = End.time_since_epoch();
		}

		// Add the processing time to the total
		Data.first += MS;

		// Check if the client has exceeded the limit
		if (Data.first >= net_chan_limit_msec->GetInt())
		{
			// Shutdown the client
			Data.first = 0;
			Data.second = End.time_since_epoch();
			Channel->Shutdown("Exceeded net processing time.");

			return false;
		}

		// Return the original value
		return Return;
	}

	Detouring::Hook::Target target;

	static void Initialize(GarrysMod::Lua::ILuaBase *LUA)
	{
		global::lua = reinterpret_cast<GarrysMod::Lua::ILuaInterface *>(LUA);

		// Register the convar using the global lua state
		global::lua->CreateConVar("net_chan_limit_msec", "0", "Netchannel processing is limited to so many milliseconds, abort connection if exceeding budget.", FCVAR_ARCHIVE | FCVAR_GAMEDLL);

		// Get a pointer to the original function
		global::ProcessMessages_original = FunctionPointers::CNetChan_ProcessMessages();

		// Check if the function exists
		if (ProcessMessages_original == nullptr)
		{
			LUA->ThrowError("failed to retrieve CNetChan::ProcessMessages");
			return;
		}

		// Create a target for the hook
		global::target = Detouring::Hook::Target((void *)ProcessMessages_original);

		// Check if the target is valid
		if (!global::target.IsValid())
		{
			LUA->ThrowError("Failed to create target");
			return;
		}

		// Create the hook
		if (!global::ProcessMessagesHook.Create(global::target, reinterpret_cast<void *>(&global::ProcessMessages_Hook)))
		{
			LUA->ThrowError("Failed to create hook");
			return;
		}

		// Enable the hook
		global::ProcessMessagesHook.Enable();
	}

	static void deinitialize()
	{
		// Disable the hook
		global::ProcessMessagesHook.Destroy();
	}

}

GMOD_MODULE_OPEN()
{
	// Initialize the global lua state
	global::Initialize(LUA);
	return 0;
}

GMOD_MODULE_CLOSE()
{
	// Deinitialize and destroy the hook
	global::deinitialize();
	return 0;
}