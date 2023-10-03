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

project.addFile('Sources/**');
project.addExclude('Sources/generators/**')

if (platform === Platform.Windows) {
	project.addDefine('_CRT_SECURE_NO_WARNINGS');
	project.addLib('d3dcompiler');
}

resolve(project);
