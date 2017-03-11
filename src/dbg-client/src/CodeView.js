import h from 'snabbdom/h';
import Union from 'union-type';
import assoc from 'ramda/src/assoc';
import CodeMirror from 'codemirror';
import 'codemirror/lib/codemirror.css';
import './CodeView.scss';

require('codemirror/mode/clojure/clojure');
require('codemirror/mode/clike/clike');

export const init = () => null;

export const Action = Union({
	InitCodeMirror: [Object],
	ViewCode: [String, String, Object]
});

export const update = Action.caseOn({
	InitCodeMirror: (codemirror, _) => codemirror,
	ViewCode: (code, mode, location, codemirror) => {
		codemirror.setOption("mode", mode);
		codemirror.setValue(code);
		codemirror.markText(
			{
				line: Math.max(0, location.start.line - 1),
				ch: Math.max(0, location.start.column - 1)
			},
			{
				line: Math.max(0, location.end.line - 1),
				ch: Math.max(0, location.end.column || 80)
			},
			{ className: "active-range" }
		);
		return codemirror;
	}
});

export const render = (model, actions$) =>
	h("textarea", {
		hook: {
			insert: (vnode) => {
				const codemirror = CodeMirror.fromTextArea(vnode.elm, {
					theme: "default",
					mode: "clike",
					lineNumbers: true,
					readOnly: true
				});
				actions$(Action.InitCodeMirror(codemirror));
			}
		}
	});
