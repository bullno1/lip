import h from 'snabbdom/h';
import Union from 'union-type';
import curry from 'ramda/src/curry';
import './CollapsibleList.scss';

export const init = () => false;

export const Action = Union({
	SetCollapsed: [Boolean]
});

export const update = curry(Action.caseOn({
	SetCollapsed: (collapsed) => collapsed
}));

export const render = (collapsed, actions$, heading, items, selectedIndex) => {
	let entries;
	if(collapsed) {
		entries = [];
	} else {
		entries = items.map((item, index) =>
			h("li.pure-menu-item", {
				class: { "pure-menu-selected":  index === selectedIndex }
			}, item)
		);
	}

	return h("div.pure-menu", [
		h("span.pure-menu-heading.collapsible-menu-heading", {
			class: { "collapsed": collapsed },
			on: { click: [ actions$, Action.SetCollapsed(!collapsed) ] }
		}, heading),
		h("ul.pure-menu-list", entries)
	]);
}
