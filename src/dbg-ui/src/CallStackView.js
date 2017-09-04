import h from 'snabbdom/h';
import assoc from 'ramda/src/assoc';
import lensProp from 'ramda/src/lensProp';
import forwardTo from 'flyd/module/forwardto';
import Union from 'union-type';
import * as Collapsible from 'Collapsible';

export const init = () => ({
	callStack: [],
	activeStackLevel: 0,
	collapsible: Collapsible.init()
});

export const Action = Union({
	SetCallStack: [Array],
	SetActiveStackLevel: [Number],
	Collapsible: [Collapsible.Action]
});

export const update = Action.caseOn({
	SetCallStack: assoc('callStack'),
	SetActiveStackLevel: assoc('activeStackLevel'),
	Collapsible: Collapsible.updateNested(lensProp('collapsible'))
});

export const render = (model, actions$) =>
	Collapsible.render(
		model.collapsible, forwardTo(actions$, Action.Collapsible),
		"Call Stack",
		h("div.pure-menu",
			h("ul.pure-menu-list", model.callStack.map((entry, index) =>
				h("li.pure-menu-item",
					{ class: { "pure-menu-selected": index === model.activeStackLevel } },
					h("a.pure-menu-link", {
						props: { href: '#' },
						on: { click: [actions$, Action.SetActiveStackLevel(index)] }
					}, entry.filename)
				)
			))
		)
	);
