import h from 'snabbdom/h';
import assoc from 'ramda/src/assoc';
import Union from 'union-type';

export const init = () => ({
	callStack: [],
	activeStackLevel: 0
});

export const Action = Union({
	SetCallStack: [Array],
	SetActiveStackLevel: [Number]
});

export const update = Action.caseOn({
	SetCallStack: assoc('callStack'),
	SetActiveStackLevel: assoc('activeStackLevel')
});

export const render = (model, actions$) =>
	h("div.pure-menu", [
		h("span.pure-menu-heading", "Call Stack"),
		h("ul.pure-menu-list", model.callStack.map((entry, index) =>
			h("li.pure-menu-item", {
				key: index,
				class: { "pure-menu-selected": index === model.activeStackLevel },
			}, [
				h("a.pure-menu-link", {
					props: { href: '#' },
					on: { click: [actions$, Action.SetActiveStackLevel(index)] }
				}, [
					entry.filename
				])
			])
		))
	]);
