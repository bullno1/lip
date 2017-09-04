import h from 'snabbdom/h';
import Union from 'union-type';
import assoc from 'ramda/src/assoc';
import pipe from 'ramda/src/pipe';
import Msgpack from 'msgpack-lite';

export const init = () => ({
	commandURL: null,
	paused: false
});

export const Command = Union({
	Step: [],
	StepInto: [],
	StepOut: [],
	StepOver: [],
	Continue: [],
	Break: []
});

export const Action = Union({
	UpdateDbg: [Object],
	SendCommand: [Command]
});

export const update = Action.caseOn({
	UpdateDbg: (dbg, model) =>
		pipe(
			assoc('paused', dbg.command === 'LIP_DBG_BREAK'),
			assoc('commandURL', dbg.getLink('command').href)
		)(model),
	SendCommand: (cmd, model) => {
		const command = Msgpack.encode(Command.case({
			StepInto: () => "step-into",
			StepOut: () => "step-out",
			StepOver: () => "step-over",
			Step: () => "step",
			Continue: () => "continue",
			Break: () => "break",
		}, cmd));
		fetch(model.commandURL, {method: "POST", body: command});
		return assoc('paused', false, model);
	}
});

export const render = (model, actions$) => {
	const stepDisabled = !model.commandURL || !model.paused;

	return h("div.pure-button-group", [
		h("button.pure-button.button-small",
			{
				props: { disabled: !model.commandURL },
				on: {click: [
					actions$,
					Action.SendCommand(model.paused ? Command.Continue() : Command.Break())
				]}
			},
			[model.paused ? h("i.icon.icon-continue") : h("i.icon.icon-break")]),
		h("button.pure-button.button-small",
			{
				props: { disabled: stepDisabled },
				on: { click: [ actions$, Action.SendCommand(Command.StepOver()) ] }
			},
			[h("i.icon.icon-step-over", "( )")]),
		h("button.pure-button.button-small",
			{
				props: { disabled: stepDisabled },
				on: { click: [ actions$, Action.SendCommand(Command.StepInto()) ] }
			},
			[h("i.icon.icon-step-into", "(  )")]),
		h("button.pure-button.button-small",
			{
				props: { disabled: stepDisabled },
				on: { click: [ actions$, Action.SendCommand(Command.StepOut()) ] }
			},
			[h("i.icon.icon-step-out", "(  )")]
		),
		h("button.pure-button.button-small",
			{
				props: { disabled: stepDisabled },
				on: { click: [ actions$, Action.SendCommand(Command.Step()) ] }
			},
			[h("i.icon.icon-step", " ")]
		)
	]);
}
