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
import * as CodeView from './CodeView';
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
	SourceView: [CodeView.Action],
	BytecodeView: [CodeView.Action],
	Toolbar: [Toolbar.Action],
	CallStackView: [CallStackView.Action],
	DisplayStackFrame: [Object]
});

export const init = () => ({
	dbg: null,
	notify$: flyd.stream(),
	activeStackLevel: 0,
	sourceView: nested(CodeView, Action.SourceView),
	bytecodeView: nested(CodeView, Action.BytecodeView),
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
				CodeView.Action.ViewCode(
					source,
					activeStackEntry.filename.endsWith('.c') ? "clike" : "clojure",
					activeStackEntry.location
				)
			))
		});

	if(!activeStackEntry.is_native) {
		activeStackEntry.getLink("self").fetch()
			.then((stackFrame) =>
				model.notify$(Action.DisplayStackFrame(stackFrame))
			);

		return model;
	} else {
		return updateNested(
			lensProp('bytecodeView'),
			CodeView.Action.ViewCode(
				'',
				'',
				{
					start: { line: 0, column: 0 },
					end: { line: 0, column: 0 }
				}
			),
			model
		);
	}
};

const refreshDbg = (model) =>
	halApp.fetch().then((dbg) => model.notify$(Action.UpdateDbg(dbg)));

const formatBytecode = (bytecode) =>
	bytecode.map((instr) => instr.join(" ")).join("\n");

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
	BytecodeView: updateNested(lensProp('bytecodeView')),
	Toolbar: updateNested(lensProp('toolbar')),
	CallStackView: (action, model) => CallStackView.Action.case({
		SetActiveStackLevel: (level) =>
			refreshSource(pipe(
				assoc('activeStackLevel', level),
				assoc('callStackView', model.callStackView.update(action)),
			)(model)),
		_: updateNested(lensProp('callStackView'), action)
	}, action),
	DisplayStackFrame: (stackFrame, model) =>
		updateNested(
			lensProp('bytecodeView'),
			CodeView.Action.ViewCode(
				formatBytecode(stackFrame.function.bytecode),
				'',
				{
					start: { line: stackFrame.pc + 1, column: 0 },
					end: { line: stackFrame.pc + 1, column: 0 }
				}
			),
			model
		)
});

export const render = (model, actions) =>
	splitPane({ direction: 'horizontal', sizes: [75, 25] }, [
		splitPane({ direction: 'vertical', sizes: [50, 50] }, [
			model.sourceView.render(actions),
			model.bytecodeView.render(actions),
		]),
		h("div", [
			model.toolbar.render(actions),
			model.callStackView.render(actions),
		])
	]);

export const subscribe = (model) => {
	refreshDbg(model);
	return model.notify$;
};
