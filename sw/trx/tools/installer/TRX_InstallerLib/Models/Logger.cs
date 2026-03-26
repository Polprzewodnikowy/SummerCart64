namespace TRX_InstallerLib.Models;

public class LogEventArgs
{
    public LogEventArgs(string message)
    {
        Message = message;
    }

    public string Message { get; }
}

public class Logger
{
    public delegate void LogEventHandler(object sender, LogEventArgs e);

    public event LogEventHandler? LogEvent;

    public void RaiseLogEvent(string message)
    {
        LogEvent?.Invoke(this, new LogEventArgs(message));
    }
}
