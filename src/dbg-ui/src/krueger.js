import flyd from 'flyd';
import flip from 'ramda/src/flip';
import over from 'ramda/src/over';
import curry from 'ramda/src/curry';
import forwardTo from 'flyd/module/forwardto';
const snabbdom = require('snabbdom');
const patch = snabbdom.init([
	require('snabbdom/modules/class').default,
	require('snabbdom/modules/props').default,
	require('snabbdom/modules/style').default,
	require('snabbdom/modules/eventlisteners').default
]);

export const mount = (module, root) => {
	const { init, update, render, subscribe } = module;

	const model = init();
	const actions$ = flyd.stream();

	if(subscribe) {
		const subscription$ = subscribe(model);
		flyd.on(actions$, subscription$);
	}

	const model$ = flyd.scan(flip(update), model, actions$);
	const vnode$ = flyd.map((model) => render(model, actions$), model$);

	flyd.scan(patch, root, vnode$);
}

const makeNested = (module, actionWrapFn, model) => ({
	model,
	update: (action) => makeNested(module, actionWrapFn, module.update(action, model)),
	render: (actions$) => module.render(model, forwardTo(actions$, actionWrapFn))
});

export const nested = (module, actionWrapFn, ...args) =>
	makeNested(module, actionWrapFn, module.init(...args));

export const updateNested = curry((lens, action, model) =>
	over(lens, (nested) => nested.update(action), model));
