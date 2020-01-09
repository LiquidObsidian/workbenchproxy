# Workbench Proxy

### A magical tool for linting (actually compiling) your EnforceScript code in VS Code.

**RECOMMENDED:** 

[Follow this tutorial first before continuing to ensure your project is set up properly.](https://github.com/maxkunes/Enscript-Workbench-Project-Setup/wiki)

Your DayZ Workbench should already be capable of compiling and reading errors for your project.

## Usage:

**[**PLEASE ENSURE YOUR PROJECT IS SET UP PROPERLY**](https://github.com/maxkunes/Enscript-Workbench-Project-Setup/wiki/3.-Workbench-Configuration)**

> Install the extension (from the vsix file)

> Open VS Code in the ***same folder*** as your **mod project folder** (the one that contains your scripts folder etc.).

> Create a `.workbenchproxy` file and leave it blank (this is to enable it for the project)

Your explorer should look like this at this point (example):

![example](https://i.imgur.com/dWvHH5D.png)

> While editing, have **DayZ Workbench** open with the **Script Editor** window open.

> When you save your files, it will magically compile your project through **Workbench** and read the errors into **VS Code**.

**You __MUST__ have the Script Editor on DayZ Workbench open for this to work.**

**Do not try to use multiple VS Code instances with WorkbenchProxy running on them at once, it will fail to connect.**

**If you close the Script Editor after compiling with this atleast once, your Workbench will crash. Sorry.**

## How does it work?

### The magic of reverse engineering and named pipes.
#### I plan to add debugging support in the future. :)

----

Due to the nature of the way this works, your anti-virus *may* flag this extension when it tries to connect to DayZ Workbench.

However, you are free read the source and try to compile all of this yourself should you want to.