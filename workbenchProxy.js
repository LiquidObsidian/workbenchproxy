const WBProxyClient = require('./workbenchProxyClient')

class WBProxy {
    constructor(path) {
        this.loader = require('./build/Release/workbenchProxyLoader.node')
        try {
            this.success = this.loader.connectInternal(path)
            this.proxy = new WBProxyClient()
        } catch(e) {
            return
        }
    }

    async connect() {
        if (!this.proxy) return false

        return await this.proxy.connect()
    }

    connected() {
        return this.success && this.proxy && this.proxy.connected
    }

    onDisconnect(cb) {
        this.proxy.disconnectCallback = cb
    }

    compile(cb) {
        this.proxy.compile(cb)
    }
}

module.exports = WBProxy