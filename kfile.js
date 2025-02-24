let project = new Project('Kongruent');
project.setExecutableName('kongruent');

let library = false;

if (library) {
	project.addDefine('KONG_LIBRARY');
}
else {
	project.setCmd();
	project.setDebugDir('tests');
}

project.addExclude('.git/**');
project.addExclude('build/**');

project.addFile('sources/**');
project.addExclude('sources/generators/**')

if (platform === Platform.Windows) {
	project.addDefine('_CRT_SECURE_NO_WARNINGS');
	project.addLib('d3dcompiler');

	project.addIncludeDir('sources/libs/dxc/inc');
	project.addLib('sources/libs/dxc/lib/x64/dxcompiler');
}

resolve(project);
