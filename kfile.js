let project = new Project('Kongruent');

let library = false;

if (library) {
	project.addDefine('KONG_LIBRARY');
}
else {
	project.setCmd();
	//project.setDebugDir('tests');
}

project.addExclude('.git/**');
project.addExclude('build/**');

project.addFile('Sources/**');

if (platform === Platform.Windows) {
	project.addLib('d3dcompiler');
}

resolve(project);
