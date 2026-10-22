(text)=
# Text and fonts

Configure a default font once, select another registered font where needed,
shape a message and retain its uploaded resources through replacement.
[Display and update text](../getting-started/how-to/update-text.md)
is the working sequence; the [Text system](../engine-systems/text.md) maps its types.

```{toctree}
:maxdepth: 1
:titlesonly:

Display and update text <../getting-started/how-to/update-text>
Configure fonts and prepare text <../assets/text-layout>
Decode UTF-8 and interpret source indices <../assets/text-encoding>
Use legacy bitmap fonts <../resources/legacy-ffont>
```

Use the Unicode path for new shaped text. Legacy atlas metrics and alternate-bank
compatibility retain their own contracts. Toolkit-owned text follows the selected
UI module's layout path, using Engine's configured default and retained fonts.
For book/note font variation, see
[named groups and saved choices](../assets/text-layout.md#choose-and-save-a-font).
