import Msgpack from 'msgpack-lite';
import Stream from 'mithril/stream';
import m from 'mithril';
import { SplitPane } from './SplitPane';
import { CallStackView } from './CallStackView';
import { SourceView } from './SourceView';
import { Toolbar } from './Toolbar';
import { HALApp } from './hal';
import { parse as parseURITemplate } from 'uri-template';

const appState = {
	dbg: Stream(),
	stackLevel: Stream(),
	sourceCode: Stream("")
};

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
			decode: resp =>
				resp
					.arrayBuffer()
					.then(buffer => Msgpack.decode(new Uint8Array(buffer)))
		},
		'text/plain': {
			isHAL: false,
			decode: resp => resp.text()
		}
	}
});

const App = {
	view: function(vnode) {
		const state = vnode.attrs.state;
		const dbg = state.dbg();
		return m(SplitPane, { direction: 'horizontal', sizes: [75, 25] }, [
			m(SourceView, {
				sourceCode: state.sourceCode(),
				filename: state.stackLevel().filename,
				location: state.stackLevel().location
			}),
			m("div", [
				m(Toolbar),
				m(CallStackView, {
					callStack: dbg.getEmbedded('vm').getEmbedded('call_stack').getEmbedded('item'),
					stackLevel: state.stackLevel
				})
			])
		])
	}
};

const root = document.body;
Stream.combine(() => m.render(root, m(App, { state: appState })), Object.values(appState));

appState.stackLevel.map(level => {
	level.getLink('src').fetch().then(appState.sourceCode);
});

halApp.fetch().then(refreshState);

function refreshState(dbg) {
	appState.dbg(dbg);
	const vm = dbg.getEmbedded('vm');
	appState.stackLevel(vm.getEmbedded('call_stack').getEmbedded('item')[0]);
}
