#include <Windows.h>
#include "../include/json.hpp"

using json = nlohmann::json;

enum workbench_proxy_action {
	compile,
	read_errors
};

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
			1024,
			1024,
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

				res[ "actionResponse" ] = actionResponse;

				send_response( res );

				break;
			}
			case workbench_proxy_action::read_errors:
			{
				printf( "%s\n", "compile" );
				break;
			}
			}
		}
	}

	void send_response( json response ) {
		send_response( response.dump( 0 ) );
	}

	void send_response( std::string response ) {
		WriteFile( m_pipe, response.data(), response.size() + 1, &write_size, NULL );
	}
private:
	HANDLE m_pipe;

	DWORD read_size;
	char buffer[ 1024 ] = { 0 };

	DWORD write_size;
};