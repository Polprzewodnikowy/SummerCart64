using System.Windows;
using TRX_InstallerLib.Models;
using WC = System.Windows.Controls;

namespace TRX_InstallerLib.Controls;

public partial class InstallStepControl : WC.UserControl
{
    public InstallStepControl()
    {
        InitializeComponent();
        DataContextChanged += (object sender, DependencyPropertyChangedEventArgs e) =>
        {
            var dataContext = DataContext as InstallStep;
            if (dataContext is not null)
            {
                string? lastMessage = null;
                dataContext.Logger.LogEvent += (object sender, LogEventArgs e) =>
                {
                    if (e.Message != lastMessage)
                    {
                        lastMessage = e.Message;
                        AppendMessage(e.Message);
                    }
                };
            }
        };
    }

    private void AppendMessage(string message)
    {
        _logTextBox.Dispatcher.Invoke(() =>
        {
            _logTextBox.AppendText(message + Environment.NewLine);
            _logTextBox.Focus();
            _logTextBox.CaretIndex = _logTextBox.Text.Length;
            _logTextBox.ScrollToEnd();
        });
    }
}
