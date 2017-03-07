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
		this.cm.setOption("mode", vnode.attrs.filename.endsWith('.c') ? "clike" : "clojure");
		this.cm.setValue(attrs.sourceCode);
		this.cm.markText(
			{
				line: Math.max(0, attrs.location.start.line - 1),
				ch: Math.max(0, attrs.location.start.column - 1)
			},
			{
				line: Math.max(0, attrs.location.end.line - 1),
				ch: Math.max(0, attrs.location.end.column || 80)
			},
			{ className: "active-range" }
		);
		console.log(
			{ line: attrs.location.start.line, ch: attrs.location.start.column },
			{ line: attrs.location.end.line, ch: attrs.location.end.column || 80 },
		);
	}
};
