#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>

#pragma comment(lib, "ntdll.lib")

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

#include "patterns.hpp"

// single header json lib by nlohmann
// for some reason intellisense breaks on certain 
// functions and i can't fix it but it compiles anyways
#include "../include/json.hpp"
using json = nlohmann::json;

DWORD get_main_thread_id() {
	const auto workbench_app = reinterpret_cast< uintptr_t >(
		GetModuleHandle( "workbenchApp.exe" ) );

	// TODO: add sig
	const auto find_start_address = workbench_app + 0x16BA3C8;

	const auto cur_pid = GetCurrentProcessId();

	HANDLE h = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if ( h != INVALID_HANDLE_VALUE ) {
		THREADENTRY32 te;
		te.dwSize = sizeof( te );
		if ( Thread32First( h, &te ) ) {
			do {
				if ( te.th32OwnerProcessID != cur_pid ) continue;

				HANDLE thread = OpenThread( THREAD_QUERY_INFORMATION, false, te.th32ThreadID );
				if ( !thread ) continue;

				uintptr_t start_address;
				NtQueryInformationThread( thread, static_cast< THREADINFOCLASS >( 9 ), &start_address, sizeof( uintptr_t ), NULL );

				CloseHandle( thread );

				if ( start_address == find_start_address ) {
					return te.th32ThreadID;
				}
			} while ( Thread32Next( h, &te ) );
		}
		CloseHandle( h );
	}

	return 0;
}

namespace workbench
{
	class error_entry {
	public:
		enum type {
			error,
			warning,
			hint
		};

		error_entry( type error_type, void* error )
			: m_type( error_type ), m_error( reinterpret_cast< uintptr_t >( error ) ) {}

		const std::string get_file_path() {
			return *reinterpret_cast< char** >( m_error + 0x40 );
		}

		const std::string get_description() {
			return *reinterpret_cast< char** >( m_error + 0x50 );
		}

		const int32_t get_line() {
			return *reinterpret_cast< int32_t* >( m_error + 0x48 );
		}

		std::string get_type_str() {
			switch ( m_type ) {
			case type::error:
				return "Error";
			case type::warning:
				return "Warning";
			case type::hint:
				return "Hint";
			}

			return "?";
		}
	private:
		type m_type;
		uintptr_t m_error;
	};

	// this is a QWindow or whatever for the script editor
	class script_editor {
	public:
		using compile_results_t = std::vector<error_entry>;

		// basically simulates you pressing F7 by calling the function directly
		int force_compile( compile_results_t& entries_out ) {
			if ( wait_for_apc )
				return -1;

			entries_out.clear();

			using fn_compile_func_t = void( __thiscall* )( script_editor * thisptr );
			static const auto _compile_project_ = reinterpret_cast< fn_compile_func_t >(
				pattern::find_rel( "workbenchApp.exe", "E9 ? ? ? ? 49 8B 41 10 48 8B CB 49 8B 51 08 44 8B 00", 0, 1, 5 ) );

			const auto main_thread_id = get_main_thread_id();
			HANDLE main_thread = OpenThread( THREAD_ALL_ACCESS, false, main_thread_id );
			if ( !main_thread ) return -2;

			// we have to queue an apc for this call because it calls QT functions which require thread local storage in the main thread
			const auto apc = QueueUserAPC( reinterpret_cast< PAPCFUNC >( compile_apc ), main_thread, reinterpret_cast< ULONG_PTR >( this ) );
			if ( !apc ) return -2;

			wait_for_apc = true;
			while ( wait_for_apc )
				std::this_thread::sleep_for( 1ms );

			// this + 76C810 = 3 QTreeWidgetItem*s which is the root for the errors, warnings, and hints.
			// it contains a pointer to a weird ass array with 2 types of entries but it's split in a weird fucking way???
			// TODO: add sig
			const auto offset = 0x76C810;

			size_t error_count = 0;

			for ( auto tree_num = 0; tree_num < 3; tree_num++ ) {
				const auto tree = *reinterpret_cast< uintptr_t* >( this + offset + ( tree_num * sizeof( uintptr_t ) ) );
				if ( !tree ) return -2;

				const auto script_errors_table = *reinterpret_cast< uintptr_t* >( tree + 0x30 );
				if ( !script_errors_table ) return -2;

				// halfway through this fucking LGBTQ array it decides to change genders and then change back LOL
				// the array might start with enf::SyntaxtTree::VarNode entires then become ScriptError entires then 
				// back to enf::SyntaxtTree::VarNode entries. wtf?? assuming this is some weird QT table.
				const auto script_errors_start = *reinterpret_cast< uint32_t* >( script_errors_table + 0x8 );
				const auto script_errors_end = *reinterpret_cast< uint32_t* >( script_errors_table + 0xC );

				for ( auto i = script_errors_start; i < script_errors_end; i++ ) {
					const auto error = *reinterpret_cast< void** >(
						script_errors_table + 0x10 + i * sizeof( uintptr_t ) );
					if ( !error ) continue;

					entries_out.push_back( error_entry( static_cast< error_entry::type >( tree_num ), error ) );
				}
			}

			return 1;
		}
	private:
		static bool wait_for_apc;

		static void compile_apc( ULONG_PTR param ) {
			using fn_compile_func_t = void( __thiscall* )( script_editor * thisptr );
			static const auto _compile_project_ = reinterpret_cast< fn_compile_func_t >(
				pattern::find_rel( "workbenchApp.exe", "E9 ? ? ? ? 49 8B 41 10 48 8B CB 49 8B 51 08 44 8B 00", 0, 1, 5 ) );

			_compile_project_( reinterpret_cast< script_editor* >( param ) );

			// sometimes it doesn't finish or something
			//std::this_thread::sleep_for( 50ms );

			wait_for_apc = false;
		}
	};

	bool script_editor::wait_for_apc = false;
}

uintptr_t workbench_app;
workbench::script_editor** script_editor_ptr;

enum workbench_proxy_action {
	compile
};

// my shitty named pipe server
class workbench_proxy_sv {
public:
	workbench_proxy_sv() {
		start();
	}

	~workbench_proxy_sv() {
		stop();
	}

	void start() {
		m_pipe = CreateNamedPipeA(
			TEXT( "\\\\.\\pipe\\WorkbenchProxy" ),
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			1,
			100000,
			100000,
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL
		);

		if ( m_pipe == INVALID_HANDLE_VALUE )
			return;

		while ( true ) {
			if ( ConnectNamedPipe( m_pipe, NULL ) != FALSE ) {
				session();
			}

			DisconnectNamedPipe( m_pipe );
		}
	}

	void stop() {
		CloseHandle( m_pipe );
	}

private:
	void session() {
		while ( ReadFile( m_pipe, buffer, sizeof( buffer ) - 1, &read_size, NULL ) != FALSE ) {
			buffer[ read_size ] = '\0';

			handler();
		}
	}

	void handler() {
		const auto v = json::parse( buffer );

		const auto _action = v[ "action" ];
		if ( !_action.is_null() ) {
			const auto action = static_cast< int >( _action.get<double>() );

			switch ( action ) {
			case workbench_proxy_action::compile:
			{
				json res;

				json actionResponse;
				actionResponse[ "action" ] = workbench_proxy_action::compile;
				actionResponse[ "success" ] = true;

				const auto script_editor = *script_editor_ptr;
				if ( !script_editor ) {
					actionResponse[ "success" ] = false;
					actionResponse[ "wasScriptEditorClosed" ] = true;
					goto compileResponse;
				}

				{
					workbench::script_editor::compile_results_t results;
					int compile_result = script_editor->force_compile( results );
					actionResponse[ "success" ] = compile_result;

					if ( compile_result != 1 ) {
						goto compileResponse;
					}

					json obj_results;

					for ( auto result : results ) {
						json obj_result;

						obj_result[ "filePath" ] = result.get_file_path();
						obj_result[ "line" ] = result.get_line();
						obj_result[ "description" ] = result.get_description();
						obj_result[ "type" ] = result.get_type_str();

						obj_results.push_back( obj_result );
					}

					actionResponse[ "errorList" ] = obj_results;
				}

			compileResponse:
				res[ "actionResponse" ] = actionResponse;

				send_response( res );

				break;
			}
			}
		}
	}

	void send_response( json response ) {
		send_response( response.dump( 0 ) );
	}

	void send_response( std::string response ) {
		WriteFile( m_pipe, response.data(), response.size(), &write_size, NULL );
	}
private:
	HANDLE m_pipe;

	DWORD read_size;
	char buffer[ 100000 ] = { 0 };

	DWORD write_size;
};

workbench_proxy_sv* srv;

DWORD start(LPVOID param) {
	workbench_app = reinterpret_cast< uintptr_t >(
		GetModuleHandle( "workbenchApp.exe" ) );

	script_editor_ptr = reinterpret_cast< workbench::script_editor * * >(
		pattern::find_rel( "workbenchApp.exe", "48 8B 0D ? ? ? ? 48 85 C9 74 77" ) );

	/*if ( !script_editor ) {
		printf( "%s\n", "[Workbench Proxy] Script editor window must be open." );
		return;
	}

	printf( "%s\n", "[Workbench Proxy] Starting Workbench compile." );

	workbench::script_editor::compile_results_t results;
	if ( script_editor->force_compile( results ) ) {
		printf( "%s\n", "[Workbench Proxy] Finished workbench compile successfully." );

		for ( auto result : results ) {
			printf( "[Workbench Proxy] [%s:%d] %s\n",
					result.get_file_path().c_str(),
					result.get_line(),
					result.get_description().c_str() );
		}
	} else {
		printf( "%s\n", "[Workbench Proxy] Compilation failed." );
	}*/

	srv = new workbench_proxy_sv;

	return 0;
}

void stop() {
	srv->stop();
	delete srv;
}

BOOL WINAPI DllMain(
  _In_ HINSTANCE hinstDLL,
  _In_ DWORD     fdwReason,
  _In_ LPVOID    lpvReserved
) {
	switch ( fdwReason ) {
	case DLL_PROCESS_ATTACH:
		CreateThread( NULL, NULL, start, NULL, NULL, NULL );
		return TRUE;
	case DLL_PROCESS_DETACH:
		stop();
	}

	return FALSE;
}