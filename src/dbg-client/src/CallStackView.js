import h from 'snabbdom/h';
import assoc from 'ramda/src/assoc';
import evolve from 'ramda/src/evolve';
import forwardTo from 'flyd/module/forwardto';
import Union from 'union-type';
import * as CollapsibleList from 'CollapsibleList';

export const init = () => ({
	callStack: [],
	activeStackLevel: 0,
	collapsibleList: CollapsibleList.init()
});

export const Action = Union({
	SetCallStack: [Array],
	SetActiveStackLevel: [Number],
	CollapsibleList: [CollapsibleList.Action]
});

export const update = Action.caseOn({
	SetCallStack: assoc('callStack'),
	SetActiveStackLevel: assoc('activeStackLevel'),
	CollapsibleList: (action, model) =>
		evolve({collapsibleList: CollapsibleList.update(action) }, model)
});

export const render = (model, actions$) =>
	CollapsibleList.render(
		model.collapsibleList, forwardTo(actions$, Action.CollapsibleList),
		"Call Stack",
		model.callStack.map((entry, index) =>
			h("a.pure-menu-link", {
				props: { href: '#' },
				on: { click: [actions$, Action.SetActiveStackLevel(index)] }
			}, [
				entry.filename
			])
		),
		model.activeStackLevel
	);
