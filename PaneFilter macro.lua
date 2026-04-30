Macro {
  description="PaneFilter starter";
  area="Shell";
  key="/.Alt(Shift)?./";
  flags="EmptyCommandLine NoPluginPanels";
  action=function()
    local k = akey(1) or ""
    Plugin.Command("704903c8-51a4-4737-a194-975104e55d30", k)
  end;
}
