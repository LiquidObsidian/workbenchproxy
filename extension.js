const vscode = require('vscode')
const WBProxy = require('./workbenchProxy')

let EXTENSIONPATH

for (let ext of vscode.extensions.all) {
	if (ext.id.indexOf('workbenchproxy') != -1) {
		EXTENSIONPATH = ext.extensionPath
	}
}

let proxy
let errorDecoration, warningDecoration, hintDecoration

async function startProxy(retry = true) {
	if (!retry && proxy) return

	proxy = new WBProxy(EXTENSIONPATH)
	if (!retry) vscode.window.setStatusBarMessage('[WorkbenchProxy] Connecting to Workbench...')
	let success = await proxy.connect()
	if (!success) {
		delete proxy
		proxy = null
		vscode.window.setStatusBarMessage('[WorkbenchProxy] Failed to connect to workbench. Retrying.')
		return startProxy(true)
	}
	vscode.window.setStatusBarMessage('[WorkbenchProxy] Workbench connected!')

	proxy.onDisconnect(() => {
		delete proxy
		proxy = null
		vscode.window.setStatusBarMessage('[WorkbenchProxy] Workbench disconnected!')
	})
}

function processErrors(errorList) {
	let errorDecorations = []
	let warningDecorations = []
	let hintDecorations = []
	
	let activeEditor = vscode.window.activeTextEditor
	let path = `${vscode.workspace.name}/${vscode.workspace.asRelativePath(activeEditor.document.uri)}`

	for (let error of errorList) {
		let matches = error.filePath.toLowerCase() == path.toLowerCase()
		if (matches) {
			let errorLine = activeEditor.document.lineAt(error.line - 1)
			
			let start = errorLine.rangeIncludingLineBreak.start.translate(
				0, errorLine.firstNonWhitespaceCharacterIndex
			)
			let end = errorLine.range.end
			
			let decoration = {
				range: new vscode.Range(
					start,
					end
				),
				hoverMessage: `${error.type}: ${error.description}`
			}

			switch (error.type) {
				case 'Error': {
					errorDecorations.push(decoration)
					break
				}
				case 'Warning': {
					warningDecorations.push(decoration)
					break
				}
				case 'Hint': {
					hintDecorations.push(decoration)
					break
				}
			}
		}
	}
	
	activeEditor.setDecorations(errorDecoration, errorDecorations)
	activeEditor.setDecorations(warningDecoration, warningDecorations)
	activeEditor.setDecorations(hintDecoration, hintDecorations)
}

function clearErrors() {
	activeEditor.setDecorations(errorDecoration, [])
	activeEditor.setDecorations(warningDecoration, [])
	activeEditor.setDecorations(hintDecoration, [])
}

async function doLinting(document) {
	if (await isDayZProject()) {
		await startProxy(false)
	}

	if (!proxy || !proxy.connected()) return

	let startTime = Number(new Date())
	vscode.window.setStatusBarMessage('[WorkbenchProxy] Compiling...')
	await proxy.compile((errorList, wasScriptEditorClosed) => {
		if (wasScriptEditorClosed) {
			vscode.window.setStatusBarMessage('[WorkbenchProxy] Could not compile! Script Editor window must be open!')
			return
		}

		let eCount = 0, wCount = 0, hCount = 0

		for (let error of errorList) {
			switch (error.type) {
				case 'Error': {
					eCount++
					break
				}
				case 'Warning': {
					wCount++
					break
				}
				case 'Hint': {
					eChCountount++
					break
				}
			}
		}

		let endTime = Number(new Date())
		vscode.window.setStatusBarMessage(
			`[WorkbenchProxy] Compiled in ${endTime - startTime}ms!`
			+ ` E:${eCount} W:${wCount} H:${hCount}`
		)
		processErrors(errorList)
	})
}

async function isDayZProject() {
	let document = vscode.window.activeTextEditor.document
	let rootPath = vscode.workspace.getWorkspaceFolder(document.uri)
	let dir = await vscode.workspace.fs.readDirectory(rootPath.uri)
	for (let entry of dir) {
		if (entry[0] == '.workbenchproxy') {
			return true
		}
	}

	return false
}

async function activate(context) {
	let activeEditor = vscode.window.activeTextEditor

	errorDecoration = vscode.window.createTextEditorDecorationType({
		//fontStyle: 'italic',
		gutterIconPath: `${EXTENSIONPATH}\\img\\error.png`,
		gutterIconSize: '80%',
		overviewRulerColor: 'red',
		overviewRulerLane: vscode.OverviewRulerLane.Right,
		textDecoration: 'red wavy underline'
	});

	warningDecoration = vscode.window.createTextEditorDecorationType({
		//fontStyle: 'italic',
		gutterIconPath: `${EXTENSIONPATH}\\img\\warning.png`,
		gutterIconSize: '80%',
		overviewRulerColor: 'yellow',
		overviewRulerLane: vscode.OverviewRulerLane.Right,
		textDecoration: 'yellow wavy underline'
	});

	hintDecoration = vscode.window.createTextEditorDecorationType({
		//fontStyle: 'italic',
		gutterIconPath: `${EXTENSIONPATH}\\img\\hint.png`,
		gutterIconSize: '80%',
		overviewRulerColor: 'white',
		overviewRulerLane: vscode.OverviewRulerLane.Right,
		textDecoration: 'white wavy underline'
	});

	vscode.window.onDidChangeActiveTextEditor(editor => {
		activeEditor = editor;
		//processErrors(errorList)
		doLinting(activeEditor.document)
	}, null, context.subscriptions);

	vscode.workspace.onDidChangeTextDocument(event => {
		if (activeEditor && event.document === activeEditor.document) {
			//clearErrors()
		}
	}, null, context.subscriptions);

	vscode.workspace.onDidSaveTextDocument(doLinting)
}

exports.activate = activate

function deactivate() {}

module.exports = {
	activate,
	deactivate
}
