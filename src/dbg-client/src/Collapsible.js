import h from 'snabbdom/h';
import Union from 'union-type';
import curry from 'ramda/src/curry';
import over from 'ramda/src/over';
import './Collapsible.scss';

export const init = () => false;

export const Action = Union({
	SetCollapsed: [Boolean]
});

export const update = curry(Action.caseOn({
	SetCollapsed: (collapsed) => collapsed
}));

export const updateNested = curry((lens, action, model) =>
	over(lens, update(action), model)
);

export const render = (collapsed, actions$, heading, body) =>
	h("div.collapsible", { class: { "collapsed": collapsed } }, [
		h("div.collapsible-heading.pure-menu-heading", {
			on: { click: [ actions$, Action.SetCollapsed(!collapsed) ] }
		}, heading),
		h("div.collapsible-body", body)
	]);
