import m from 'mithril';

export const Toolbar = {
	view: function(vnode) {
		return m("div.pure-button-group", [
			m("button.pure-button.button-small", [m("i.icon.icon-run")]),
			m("button.pure-button.button-small", [m("i.icon.icon-step-over", "( )")]),
			m("button.pure-button.button-small", [m("i.icon.icon-step-in", "(  )")]),
			m("button.pure-button.button-small", [m("i.icon.icon-step-out", "(  )")])
		])
	}
};
