import h from 'snabbdom/h';
import flyd from 'flyd';
import lensProp from 'ramda/src/lensProp';
import evolve from 'ramda/src/evolve';
import pipe from 'ramda/src/pipe';
import assoc from 'ramda/src/assoc';
import Union from 'union-type';
import Msgpack from 'msgpack-lite';
import { HALApp } from './hal';
import { nested, updateNested } from './krueger';
import { splitPane } from './splitPane';
import * as SourceView from './SourceView';
import * as Toolbar from './Toolbar';
import * as CallStackView from './CallStackView';

const halApp = new HALApp({
	entrypoint: 'http://localhost:8081/dbg',
	custom_rels: {
		call_stack: "http://lip.bullno1.com/hal/relations/call_stack",
		src: "http://lip.bullno1.com/hal/relations/src",
		vm: "http://lip.bullno1.com/hal/relations/vm"
	},
	codecs: {
		'application/hal+msgpack': {
			isHAL: true,
			decode: (resp) =>
				resp
					.arrayBuffer()
					.then(buffer => Msgpack.decode(new Uint8Array(buffer)))
		},
		'text/plain': {
			isHAL: false,
			decode: (resp) => resp.text()
		}
	}
});

export const Action = Union({
	UpdateDbg: [Object],
	SourceView: [SourceView.Action],
	Toolbar: [Toolbar.Action],
	CallStackView: [CallStackView.Action]
});

export const init = () => ({
	dbg: null,
	notify$: flyd.stream(),
	activeStackLevel: 0,
	sourceView: nested(SourceView, Action.SourceView),
	toolbar: nested(Toolbar, Action.Toolbar),
	callStackView: nested(CallStackView, Action.CallStackView)
});

const refreshSource = (model) => {
	const vm = model.dbg.getEmbedded('vm');
	const callStack = vm.getEmbedded('call_stack').getEmbedded('item');
	const activeStackEntry = callStack[model.activeStackLevel];
	activeStackEntry.getLink("src").fetch()
		.then((source) => {
			model.notify$(Action.SourceView(
				SourceView.Action.ViewSource(
					source,
					activeStackEntry.filename,
					activeStackEntry.location
				)
			))
		});

	return model;
};

export const update = Action.caseOn({
	UpdateDbg: (dbg, model) => {
		const vm = dbg.getEmbedded('vm');
		const callStack = vm.getEmbedded('call_stack').getEmbedded('item');

		return refreshSource(evolve({
			dbg: (_) => dbg,
			callStackView: (callStackView) =>
				callStackView.update(CallStackView.Action.SetCallStack(callStack))
		}, model));
	},
	SourceView: updateNested(lensProp('sourceView')),
	Toolbar: updateNested(lensProp('toolbar')),
	CallStackView: (action, model) => CallStackView.Action.case({
		SetActiveStackLevel: (level) =>
			refreshSource(pipe(
				assoc('activeStackLevel', level),
				assoc('callStackView', model.callStackView.update(action)),
			)(model)),
		_: updateNested(lensProp('callStackView'), action)
	}, action)
});

export const render = (model, actions) =>
	splitPane({ direction: 'horizontal', sizes: [75, 25] }, [
		model.sourceView.render(actions),
		h("div", [
			model.toolbar.render(actions),
			model.callStackView.render(actions),
		])
	]);

export const subscribe = (model) => {
	halApp.fetch().then((dbg) => model.notify$(Action.UpdateDbg(dbg)));
	return model.notify$;
};
