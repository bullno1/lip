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
import * as ValueListView from './ValueListView';

const halApp = new HALApp({
	entrypoint: 'http://localhost:8081/dbg',
	custom_rels: {
		call_stack: "http://lip.bullno1.com/hal/relations/call_stack",
		command: "http://lip.bullno1.com/hal/relations/command",
		src: "http://lip.bullno1.com/hal/relations/src",
		status: "http://lip.bullno1.com/hal/relations/status",
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

export const init = () => ({
	dbg: null,
	ws: null,
	wsURL: null,
	notify$: flyd.stream(),
	activeStackLevel: 0,
	sourceView: nested(CodeView, Action.SourceView),
	bytecodeView: nested(CodeView, Action.BytecodeView),
	toolbar: nested(Toolbar, Action.Toolbar),
	callStackView: nested(CallStackView, Action.CallStackView),
	operandStackView: nested(ValueListView, Action.OperandStackView, "Operand Stack"),
	localView: nested(ValueListView, Action.LocalView, "Locals"),
	closureView: nested(ValueListView, Action.ClosureView, "Closure")
});

export const Action = Union({
	UpdateDbg: [Object],
	ConnectWS: [String],
	SourceView: [CodeView.Action],
	BytecodeView: [CodeView.Action],
	Toolbar: [Toolbar.Action],
	CallStackView: [CallStackView.Action],
	OperandStackView: [ValueListView.Action],
	LocalView: [ValueListView.Action],
	ClosureView: [ValueListView.Action],
	DisplayStackFrame: [Object]
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

const connectWS = (wsURL, model) => {
	const hasWS = !!model.ws && (model.ws.readyState === WebSocket.CONNECTING || model.ws.readyState === WebSocket.OPEN);
	const wsURLChanged = wsURL !== model.wsURL;

	if(hasWS && !wsURLChanged) { return model; }

	if(hasWS) { model.ws.close(); }

	const ws = new WebSocket(wsURL);
	ws.binaryType = 'arraybuffer';
	model = pipe(assoc('ws', ws), assoc('wsURL', wsURL))(model);

	ws.onopen = () => refreshDbg(model);
	ws.onmessage = (event) => {
		const dbg = halApp.parse(Msgpack.decode(new Uint8Array(event.data)));
		model.notify$(Action.UpdateDbg(dbg));
	}
	ws.onerror = ws.onclose = (error) => {
		setTimeout(() => model.notify$(Action.ConnectWS(wsURL)), 3000);
	}

	return model;
};

export const update = Action.caseOn({
	UpdateDbg: (dbg, model) => {
		const vm = dbg.getEmbedded('vm');
		const callStack = vm.getEmbedded('call_stack').getEmbedded('item');

		const wsURL = dbg.getLink('status').href;
		model = connectWS(wsURL, model);

		return refreshSource(evolve({
			wsURL: (_) => wsURL,
			dbg: (_) => dbg,
			callStackView: (callStackView) =>
				callStackView.update(CallStackView.Action.SetCallStack(callStack)),
			toolbar: (toolbar) =>
				toolbar.update(Toolbar.Action.UpdateDbg(dbg))
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
		_: () => updateNested(lensProp('callStackView'), action, model)
	}, action),
	OperandStackView: updateNested(lensProp('operandStackView')),
	LocalView: updateNested(lensProp('localView')),
	ClosureView: updateNested(lensProp('closureView')),
	DisplayStackFrame: (stackFrame, model) => {
		const loc = { line: stackFrame.pc, column: 0 };
		return pipe(
			updateNested(
				lensProp('bytecodeView'),
				CodeView.Action.ViewCode(
					formatBytecode(stackFrame.function.bytecode),
					'',
					{ start: loc, end: loc }
				)
			),
			updateNested(
				lensProp('operandStackView'),
				ValueListView.Action.SetValueList(stackFrame.function.stack)
			),
			updateNested(
				lensProp('localView'),
				ValueListView.Action.SetValueList(stackFrame.function.locals)
			),
			updateNested(
				lensProp('closureView'),
				ValueListView.Action.SetValueList(stackFrame.function.env)
			)
		)(model);
	},
	ConnectWS: connectWS
});

export const render = (model, actions) =>
	splitPane({ direction: 'horizontal', sizes: [70, 30] }, [
		splitPane({ direction: 'vertical', sizes: [50, 50] }, [
			model.sourceView.render(actions),
			model.bytecodeView.render(actions),
		]),
		h("div", [
			model.toolbar.render(actions),
			model.callStackView.render(actions),
			model.operandStackView.render(actions),
			model.localView.render(actions),
			model.closureView.render(actions)
		])
	]);

export const subscribe = (model) => {
	refreshDbg(model);
	return model.notify$;
};
