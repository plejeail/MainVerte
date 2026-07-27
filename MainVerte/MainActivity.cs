namespace MainVerte;

#if ANDROID
using System.Collections.Generic;
using System.Linq;
using _Microsoft.Android.Resource.Designer;
using Android.App;
using Android.OS;
using Activity = Android.App.Activity;

[Activity(Label = "@string/app_name", MainLauncher = true)]
public class MainActivity : Activity
{
    protected override void OnCreate(Bundle? savedInstanceState)
    {
        base.OnCreate(savedInstanceState);

        // Set our view from the "main" layout resource
        SetContentView(ResourceConstant.Layout.activity_main);
    }
}
#else
static class Program
{
    public static void Main(string[] args) {}
}
#endif
