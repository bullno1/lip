import h from 'snabbdom/h';

export const init = () => null;

export const Action = () => true;

export const update = () => null;

export const render = () =>
	h("div.pure-button-group", [
		h("button.pure-button.button-small", [h("i.icon.icon-run")]),
		h("button.pure-button.button-small", [h("i.icon.icon-step-over", "( )")]),
		h("button.pure-button.button-small", [h("i.icon.icon-step-in", "(  )")]),
		h("button.pure-button.button-small", [h("i.icon.icon-step-out", "(  )")])
	]);
