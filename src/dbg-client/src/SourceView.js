import m from 'mithril';
import CodeMirror from 'codemirror';
import 'codemirror/lib/codemirror.css';
import './SourceView.scss';

require('codemirror/mode/clojure/clojure');
require('codemirror/mode/clike/clike');

export const SourceView = {
	view: function(vnode) {
		return m("textarea", {
			oncreate: (codeVnode) => {
				this.cm = CodeMirror.fromTextArea(codeVnode.dom, {
					theme: "default",
					mode: "clike",
					lineNumbers: true,
					readOnly: true
				});
			}
		});
	},
	onbeforeupdate: function(vnode) {
		const attrs = vnode.attrs;
		const loc = attrs.location;
		this.cm.setOption("mode", attrs.filename.endsWith('.c') ? "clike" : "clojure");
		this.cm.setValue(attrs.sourceCode);
		this.cm.markText(
			{
				line: Math.max(0, loc.start.line - 1),
				ch: Math.max(0, loc.start.column - 1)
			},
			{
				line: Math.max(0, loc.end.line - 1),
				ch: Math.max(0, loc.end.column || 80)
			},
			{ className: "active-range" }
		);
	}
};
