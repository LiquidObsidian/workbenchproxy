const net = require('net')

const WBPROXY_ACTION_COMPILE = 0;

const PIPE_NAME = 'WorkbenchProxy'
const PIPE_PATH = `\\\\.\\pipe\\${PIPE_NAME}`

class WBProxyClient {
    constructor() {}

    async connect() {
        return new Promise((resolve, reject) => {
            this.connected = false

            this.connecting = true
            setTimeout(() => {
                if (this.connecting) {
                    resolve(false)
                }
            }, 1000)

            this.client = net.connect(PIPE_PATH, () => {
                this.connected = true
                this.connecting = false
                resolve(true)
                console.log('[WorkbenchProxy] Connected to proxy.')
            })
    
            this.client.on('data', (dataBuffer) => {
                if (!dataBuffer) return
                const data = JSON.parse(dataBuffer.toString('ascii'))

                if (!data) return
                if (!data.actionResponse) return
    
                switch (data.actionResponse.action) {
                    case WBPROXY_ACTION_COMPILE: {
                        if (this.compileCallback) {
                            this.compileCallback(data.actionResponse.errorList, data.actionResponse.wasScriptEditorClosed)
                        }
                        break
                    }
                }
            })
    
            this.client.on('end', () => {
                this.connected = false
                if (this.disconnectCallback) {
                    this.disconnectCallback()
                }
                console.log('[WorkbenchProxy] Disconnected from proxy.')
            })
        })
    }

    send(data) {
        this.client.write(JSON.stringify(data))
    }

    compile(cb) {
        this.compileCallback = cb
        this.send({
            'action': WBPROXY_ACTION_COMPILE
        })
    }
}

module.exports = WBProxyClient