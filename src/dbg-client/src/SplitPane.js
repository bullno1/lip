import m from 'mithril';
import Split from 'split.js';
import './SplitPane.scss';
import classNames from 'classnames';

export const SplitPane = {
	oninit: function(vnode) {
		this.childElems = new Array(vnode.children.length);
		this.count = 0;
	},
	view: function(vnode) {
		return m("div.full-height",
			vnode.children.map((child, index) => m("div", {
				class: classNames("split", "split-" + vnode.attrs.direction),
				oncreate: SplitPane._onChildCreated.bind(this, index, vnode),
			}, child))
		);
	},
	_onChildCreated: function(index, parentVnode, vnode) {
		this.childElems[index] = vnode.dom;
		if(++this.count == this.childElems.length) {
			Split(this.childElems, parentVnode.attrs);
		}
	}
};
