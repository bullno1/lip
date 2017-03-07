import Msgpack from 'msgpack-lite';
import Stream from 'mithril/stream';
import m from 'mithril';
import { SplitPane } from './SplitPane';
import { CallStackView } from './CallStackView';
import { SourceView } from './SourceView';
import { Toolbar } from './Toolbar';

const appState = {
	callStack: Stream(),
	stackLevel: Stream(),
	sourceCode: Stream("")
};

appState.stackLevel.map(level => {
	fetch('//localhost:8081/src/' + level.filename)
		.then(resp => resp.text())
		.then(appState.sourceCode);
});

fetch('//localhost:8081/vm/cs')
	.then(resp => resp.arrayBuffer())
	.then(buffer => Msgpack.decode(new Uint8Array(buffer)))
	.then(appState.callStack)
	.then(callStack => appState.stackLevel(callStack[0]));

const App = {
	view: function(vnode) {
		const state = vnode.attrs.state;
		return m(SplitPane, { direction: 'horizontal', sizes: [75, 25] }, [
			m(SourceView, {
				sourceCode: appState.sourceCode(),
				filename: appState.stackLevel().filename,
				location: appState.stackLevel().location
			}),
			m("div", [
				m(Toolbar),
				m(CallStackView, {
					callStack: state.callStack(),
					stackLevel: state.stackLevel
				})
			])
		])
	}
};

const root = document.body;
Stream.combine(() => m.render(root, m(App, { state: appState })), Object.values(appState));
