// golden capture build: argv from ?scene=<name> (tools/run_web_goldens.mjs)
var __scene = new URLSearchParams(location.search).get('scene') || 'clear';
Module['arguments'] = [
    '--capture-frame', '2',
    '--capture-output', '/captures/' + __scene + '.png',
    '/goldens/' + __scene,
];
