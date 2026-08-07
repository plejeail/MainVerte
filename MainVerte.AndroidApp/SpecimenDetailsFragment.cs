using System;
using System.Threading.Tasks;
using Android.Content;
using Android.Content.PM;
using Android.Graphics;
using Android.Graphics.Drawables;
using Android.OS;
using Android.Provider;
using Android.Views;
using Android.Widget;
using AndroidX.Activity.Result;
using AndroidX.Activity.Result.Contract;
using AndroidX.Fragment.App;
using AndroidX.Lifecycle;

using AlertDialog = Android.App.AlertDialog;
using AndroidActivityFlags = Android.Content.ActivityFlags;
using Dialog = Android.App.Dialog;
using Result = Android.App.Result;
using AndroidUri = Android.Net.Uri;

using MainVerte.Core;

namespace MainVerte.AndroidApp;


enum SpecimenDetailsMode { Read, Edit, Create, }

enum SpecimenDetailsOperationState
{
    Idle,
    Saving,
    Deleting,
    ImportingPhoto,
}

sealed class SpecimenDetailsViewModel : ViewModel
{
    private Task<SpecimenDetail?>? _loadTask;
    private bool _cleared;
    private bool _initialized;
    public MainVerteId CollectionId = MainVerteId.Invalid;
    public MainVerteId SpecimenId   = MainVerteId.Invalid;
    public SpecimenDetailsMode Mode;
    public SpecimenDetailsOperationState OperationState;
    public readonly PhotoEditSession PhotoSession = new();
    public readonly SpecimenEditor SpecimenEditor = new(Services.Database);
    public string DraftDisplayName = String.Empty;

    public bool IsBusy() {
        return OperationState == SpecimenDetailsOperationState.Saving
            || OperationState == SpecimenDetailsOperationState.Deleting;
    }

    public event Action? StateChanged;

    public void Initialize(SpecimenDetailsMode mode, MainVerteId id)
    {
        if (_initialized) {
            return;
        }

        _initialized = true;
        Mode = mode;
        if (mode == SpecimenDetailsMode.Create) {
            CollectionId = id;
            SpecimenEditor.StartNew(CollectionId);
        } else {
            SpecimenId = id;
        }

        DraftDisplayName = SpecimenEditor.Specimen?.DisplayName ?? String.Empty;
    }

    public Task<SpecimenDetail?> LoadAsync()
    {
        if (_loadTask != null) {
            return _loadTask;
        }

        _loadTask = LoadCoreAsync();
        return _loadTask;
    }

    public void SetDraftDisplayName(string displayName)
    {
        Require.NotNull(displayName);
        DraftDisplayName = displayName;
    }

    public void EnterEditMode()
    {
        if (OperationState != SpecimenDetailsOperationState.Idle || SpecimenEditor.Specimen == null) {
            return;
        }

        Mode = SpecimenDetailsMode.Edit;
        PhotoSession.ResetToOriginal();
        DraftDisplayName = SpecimenEditor.Specimen.DisplayName;
        NotifyStateChanged();
    }

    public Task ImportGalleryPhotoAsync(ContentResolver resolver, AndroidUri sourceUri)
    {
        Require.NotNull(resolver);
        Require.NotNull(sourceUri);

        if (OperationState != SpecimenDetailsOperationState.Idle) {
            return Task.CompletedTask;
        }

        SetOperationState(SpecimenDetailsOperationState.ImportingPhoto);
        return ImportGalleryPhotoCoreAsync(resolver, sourceUri);
    }

    public Task SaveAsync()
    {
        if (OperationState != SpecimenDetailsOperationState.Idle) {
            return Task.CompletedTask;
        }

        SetOperationState(SpecimenDetailsOperationState.Saving);
        return SaveCoreAsync();
    }

    public Task<bool> DeleteAsync()
    {
        if (OperationState != SpecimenDetailsOperationState.Idle) {
            return Task.FromResult(false);
        }

        SpecimenDetail? specimen = SpecimenEditor.Specimen;
        if (specimen == null) {
            return Task.FromResult(false);
        }

        SetOperationState(SpecimenDetailsOperationState.Deleting);
        return DeleteCoreAsync(specimen);
    }

    public bool CancelChanges()
    {
        if (IsBusy()) {
            return false;
        }

        PhotoSession.CancelChanges();
        SpecimenEditor.Cancel();
        DraftDisplayName = SpecimenEditor.Specimen?.DisplayName ?? String.Empty;

        if (Mode != SpecimenDetailsMode.Create) {
            Mode = SpecimenDetailsMode.Read;
        }

        SetOperationState(SpecimenDetailsOperationState.Idle);
        return true;
    }

    public void RemovePhoto()
    {
        if (OperationState != SpecimenDetailsOperationState.Idle) {
            return;
        }

        PhotoSession.RemovePhoto(Mode == SpecimenDetailsMode.Edit);
        NotifyStateChanged();
    }

    protected override void OnCleared()
    {
        _cleared = true;
        if (OperationState == SpecimenDetailsOperationState.Idle) {
            PhotoSession.CleanupUncommittedPhotoFiles();
        }

        StateChanged = null;
        base.OnCleared();
    }

    private async Task<SpecimenDetail?> LoadCoreAsync()
    {
        try {
            SpecimenDetail? specimen = await SpecimenEditor.LoadAsync(SpecimenId);
            if (specimen != null) {
                PhotoSession.SetOriginalPhoto(specimen.PhotoUri);
                DraftDisplayName = specimen.DisplayName;
            }

            NotifyStateChanged();
            return specimen;
        } catch {
            NotifyStateChanged();
            throw;
        }
    }

    private async Task ImportGalleryPhotoCoreAsync(ContentResolver resolver, AndroidUri sourceUri)
    {
        try {
            await PhotoSession.ImportGalleryPhotoAsync(resolver, sourceUri);
        } finally {
            SetOperationState(SpecimenDetailsOperationState.Idle);
        }
    }

    private async Task SaveCoreAsync()
    {
        bool databaseSaved = false;
        try {
            SpecimenDetail? specimen = SpecimenEditor.Specimen;
            if (specimen == null) {
                throw new InvalidOperationException("Specimen has not been loaded.");
            }

            string? oldPhotoUri = SpecimenEditor.IsNew ? null : specimen.PhotoUri;
            string? savedPhotoUri = PhotoSession.PrepareForSave();
            SpecimenEditor.UpdateDraft(DraftDisplayName, savedPhotoUri);
            await SpecimenEditor.SaveAsync();
            databaseSaved = true;

            PhotoSession.Commit(savedPhotoUri, oldPhotoUri);
            Mode = SpecimenDetailsMode.Read;
            NotifyStateChanged();
        } catch {
            if (!databaseSaved) {
                PhotoSession.RestorePreparedPhotoAfterFailure();
            }

            throw;
        } finally {
            SetOperationState(SpecimenDetailsOperationState.Idle);
        }
    }

    private async Task<bool> DeleteCoreAsync(SpecimenDetail specimen)
    {
        try {
            bool deleted = await SpecimenEditor.DeleteAsync();
            if (deleted) {
                PhotoStorage.DeleteOwnedFinalPhoto(specimen.PhotoUri);
            }

            return deleted;
        } finally {
            SetOperationState(SpecimenDetailsOperationState.Idle);
        }
    }

    private void SetOperationState(SpecimenDetailsOperationState state)
    {
        OperationState = state;
        NotifyStateChanged();

        if (_cleared && state == SpecimenDetailsOperationState.Idle) {
            PhotoSession.CleanupUncommittedPhotoFiles();
        }
    }

    private void NotifyStateChanged()
    {
        StateChanged?.Invoke();
    }
}

sealed class SpecimenDetailsFragment : Fragment
{
    private const string ModeArgument = "mode";
    private const string SpecimenIdArgument = "specimen_id";
    private const string CollectionIdArgument = "collection_id";
    private enum ItemId { Edit = 1, Save = 2, Cancel = 3, Delete = 4, }

    private enum PhotoAction
    {
        Gallery,
        Camera,
        Delete,
    }

    private Binding.fragment_specimen_details? _binding;
    private SpecimenDetailsViewModel _viewModel = null!;

    private Dialog? _photoFullscreenDialog;

    private ActivityResultLauncher _galleryLauncher = null!;
    private ActivityResultLauncher _cameraLauncher = null!;

    private sealed class ResultCallback(Action<Java.Lang.Object?> callback)
        : Java.Lang.Object, IActivityResultCallback
    {
        private readonly Action<Java.Lang.Object?> _callback = callback;

        public void OnActivityResult(Java.Lang.Object? result) {
            _callback(result);
        }
    }

    public static SpecimenDetailsFragment ForSpecimen(MainVerteId specimenId) {
        var fragment = new SpecimenDetailsFragment {
            Arguments = new Bundle(),
        };

        fragment.Arguments.PutInt(SpecimenIdArgument, specimenId.Value);
        fragment.Arguments.PutInt(ModeArgument, (int)SpecimenDetailsMode.Read);
        return fragment;
    }

    public static SpecimenDetailsFragment ForNewSpecimen(MainVerteId collectionId) {
        var fragment = new SpecimenDetailsFragment {
            Arguments = new Bundle(),
        };

        fragment.Arguments.PutInt(CollectionIdArgument, collectionId.Value);
        fragment.Arguments.PutInt(ModeArgument, (int)SpecimenDetailsMode.Create);
        return fragment;
    }

    public override void OnCreate(Bundle? savedInstanceState) {
        _galleryLauncher = RegisterForActivityResult(new ActivityResultContracts.StartActivityForResult(),
                                                     new ResultCallback(HandleGalleryResult));
        _cameraLauncher = RegisterForActivityResult(new ActivityResultContracts.StartActivityForResult(),
                                                    new ResultCallback(HandleCameraResult));
        base.OnCreate(savedInstanceState);

        _viewModel = new ViewModelProvider(this)
            .Get(Java.Lang.Class.FromType(typeof(SpecimenDetailsViewModel))) as SpecimenDetailsViewModel
            ?? throw new InvalidOperationException("Could not create specimen details view model.");
        _viewModel.StateChanged += HandleViewModelStateChanged;

        SpecimenDetailsMode mode = ReadModeArgument();
        MainVerteId id = ReadIdArgument(mode == SpecimenDetailsMode.Create
                                        ? CollectionIdArgument
                                        : SpecimenIdArgument);
        _viewModel.Initialize(mode, id);
    }

    public override View OnCreateView(LayoutInflater inflater, ViewGroup? container, Bundle? savedInstanceState) {
        Require.NotNull(inflater);

        View? root = inflater.Inflate(Resource.Layout.fragment_specimen_details, container, false);
        if (root == null) {
            throw new InvalidOperationException("Failed to inflate specimen details.");
        }

        _binding = new Binding.fragment_specimen_details(root);
        return root;
    }

    public override void OnViewCreated(View view, Bundle? savedInstanceState) {
        Require.NotNull(_binding);

        base.OnViewCreated(view, savedInstanceState);
        _binding!.specimen_photo_edit.Click += (_, _) => ShowPhotoActions();
        _binding.specimen_image.Click += (_, _) => ShowPhotoFullscreen();
        _binding.specimen_name_editor.TextChanged += HandleSpecimenNameTextChanged;

        if (_viewModel.Mode == SpecimenDetailsMode.Create) {
            Render();
            return;
        }

        _ = LoadSpecimenAsync();
    }

    public override void OnResume() {
        base.OnResume();
        UpdateToolbar();
    }

    private void HandleGalleryResult(Java.Lang.Object? result) {
        var activityResult = result as ActivityResult;
        if (activityResult == null || activityResult.ResultCode != (int)Result.Ok) {
            return;
        }

        Intent? data = activityResult.Data;
        AndroidUri? selectedUri = data?.Data;
        if (selectedUri != null) {
            _ = ImportGalleryPhotoAsync(selectedUri);
        }
    }

    private void HandleCameraResult(Java.Lang.Object? result) {
        var activityResult = result as ActivityResult;
        if (activityResult == null) {
            return;
        }

        CompleteCameraCapture(activityResult.ResultCode);
    }

    public override void OnDestroyView() {
        ClosePhotoFullscreen();

        if (_binding != null) {
            _binding.specimen_name_editor.TextChanged -= HandleSpecimenNameTextChanged;
        }

        _binding = null;
        base.OnDestroyView();
    }

    public override void OnDestroy() {
        _viewModel.StateChanged -= HandleViewModelStateChanged;
        base.OnDestroy();
    }

    internal bool HandleBackNavigation() {
        if (_viewModel.IsBusy()) {
            return true;
        }

        if (_viewModel.OperationState == SpecimenDetailsOperationState.ImportingPhoto) {
            CancelChanges();
            return true;
        }

        if (_viewModel.Mode != SpecimenDetailsMode.Read) {
            CancelChanges();
            return true;
        }

        return false;
    }

    private async Task LoadSpecimenAsync() {
        Require.NotNull(_binding);

        try {
            SpecimenDetail? specimen = await _viewModel.LoadAsync();
            if (_binding == null || specimen == null) {
                return;
            }

            Render();
        } catch (Exception ex) {
            Log.Warn($"Could not load specimen details: {ex.Message}");
        }
    }

    private async Task DeleteSpecimenAsync() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle) {
            return;
        }

        SpecimenDetail? specimen = _viewModel.SpecimenEditor.Specimen;
        if (specimen == null) {
            return;
        }

        try {
            bool deleted = await _viewModel.DeleteAsync();
            if (!deleted) {
                return;
            }

            if (IsAdded) {
                ParentFragmentManager.PopBackStack();
            }
        } catch (Exception ex) {
            Log.Warn($"Could not delete specimen: {ex.Message}");
        }
    }

    private void DeleteSpecimen() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle
            || _viewModel.SpecimenEditor.Specimen == null
            || Activity == null) {
            return;
        }

        AlertDialog.Builder builder = new(Activity);
        builder.SetTitle(Resource.String.specimen_detail_delete_confirmation_title);
        builder.SetMessage(Resource.String.specimen_detail_delete_confirmation_message);
        builder.SetNegativeButton(Resource.String.specimen_detail_delete_confirmation_no, (_, _) => { });
        builder.SetPositiveButton(Resource.String.specimen_detail_delete_confirmation_yes,
                                  (_, _) => {
                                      _ = DeleteSpecimenAsync();
                                      if (Activity != null) {
                                          Feedback.Send(Activity, GetString(Resource.String.specimen_detail_delete_confirmation_yes), UserFeedbackKind.Success);
                                      }
                                  });
        builder.Show();
    }

    private void EnterEditMode() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle
            || _viewModel.SpecimenEditor.Specimen == null) {
            return;
        }

        _viewModel.EnterEditMode();
    }

    private void ShowPhotoActions() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle
            || _viewModel.Mode == SpecimenDetailsMode.Read
            || Activity == null) {
            return;
        }

        string[] labels;
        PhotoAction[] actions;
        if (_viewModel.PhotoSession.HasPhoto()) {
            labels = [
                GetString(Resource.String.specimen_photo_action_gallery),
                GetString(Resource.String.specimen_photo_action_camera),
                GetString(Resource.String.specimen_photo_action_delete),
            ];

            actions = [PhotoAction.Gallery, PhotoAction.Camera, PhotoAction.Delete];
        } else {
            labels = [
                GetString(Resource.String.specimen_photo_action_gallery),
                GetString(Resource.String.specimen_photo_action_camera),
            ];

            actions = [PhotoAction.Gallery, PhotoAction.Camera];
        }

        AlertDialog.Builder builder = new(Activity);
        builder.SetTitle(Resource.String.specimen_photo_action_title);
        builder.SetItems(labels, (_, args) => ExecutePhotoAction(actions[args.Which]));
        builder.SetNegativeButton(Resource.String.toolbar_menu_action_cancel, (_, _) => { });
        builder.Show();
    }

    private void ExecutePhotoAction(PhotoAction action) {
        switch (action) {
        case PhotoAction.Gallery: StartGalleryPicker();                   break;
        case PhotoAction.Camera:  LaunchCamera();                         break;
        case PhotoAction.Delete:  RemovePhotoFromDraft();                 break;
        default:                  Log.Error($"Unknown action: {action}"); break;
        }
    }

    private void StartGalleryPicker() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle) {
            return;
        }

        Context context = RequireContext();
        PackageManager? packageManager = context.PackageManager;
        if (packageManager == null) {
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_selection_failed), UserFeedbackKind.Failure);
            }

            return;
        }

        Intent? picker = null;
        if (Build.VERSION.SdkInt >= BuildVersionCodes.Tiramisu) {
            Intent photoPicker = new("android.provider.action.PICK_IMAGES");
            if (photoPicker.ResolveActivity(packageManager) != null) {
                picker = photoPicker;
            }
        }

        if (picker == null) {
            picker = new Intent(Intent.ActionOpenDocument);
            picker.AddCategory(Intent.CategoryOpenable);
        }

        picker.SetType("image/*");
        picker.AddFlags(AndroidActivityFlags.GrantReadUriPermission);
        if (picker.ResolveActivity(packageManager) == null) {
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_selection_failed), UserFeedbackKind.Failure);
            }
            return;
        }

        _galleryLauncher.Launch(picker);
    }

    private async Task ImportGalleryPhotoAsync(AndroidUri sourceUri) {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle) {
            return;
        }

        try {
            ContentResolver? resolver = RequireContext().ContentResolver;
            if (resolver == null) {
                throw new InvalidOperationException("The content resolver is unavailable.");
            }

            await _viewModel.ImportGalleryPhotoAsync(resolver, sourceUri);
        } catch (System.OperationCanceledException) {
            // Cancellation is expected when the fragment view is left during the copy.
        } catch (Exception ex) {
            Log.Warn($"Could not import selected photo: {ex.Message}");
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_selection_failed), UserFeedbackKind.Failure);
            }
        }

        RenderPhoto();
    }

    private void LaunchCamera() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle) {
            return;
        }

        Context context = RequireContext();
        PackageManager? packageManager = context.PackageManager;
        if (packageManager == null) {
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_camera_unavailable), UserFeedbackKind.Failure);
            }

            return;
        }

        Intent camera = new(MediaStore.ActionImageCapture);
        if (camera.ResolveActivity(packageManager) == null) {
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_camera_unavailable), UserFeedbackKind.Failure);
            }

            return;
        }

        try {
            AndroidUri outputUri = _viewModel.PhotoSession.BeginCameraCapture();
            camera.PutExtra(MediaStore.ExtraOutput, outputUri);
            camera.AddFlags(AndroidActivityFlags.GrantReadUriPermission | AndroidActivityFlags.GrantWriteUriPermission);
            _cameraLauncher.Launch(camera);
        } catch (Exception ex) {
            Log.Warn($"Could not start camera capture: {ex.Message}");
            _viewModel.PhotoSession.CancelCameraCapture();

            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_camera_unavailable), UserFeedbackKind.Failure);
            }
        }
    }

    private void CompleteCameraCapture(int resultCode) {
        PhotoCaptureResult captureResult = _viewModel.PhotoSession.CompleteCameraCapture(resultCode == (int)Result.Ok);
        if (captureResult == PhotoCaptureResult.Failed) {
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_photo_capture_failed), UserFeedbackKind.Failure);
            }
        }

        if (captureResult == PhotoCaptureResult.Succeeded) {
            RenderPhoto();
        }
    }

    private void RemovePhotoFromDraft() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle) {
            return;
        }

        _viewModel.RemovePhoto();
    }

    private async Task SaveAsync() {
        if (_viewModel.OperationState != SpecimenDetailsOperationState.Idle || _binding == null) {
            return;
        }

        string displayName = _binding.specimen_name_editor.Text?.Trim() ?? String.Empty;
        if (displayName.Length == 0) {
            _binding.specimen_name_editor.Error = GetString(Resource.String.specimen_detail_mandatory);
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_detail_name_mandatory), UserFeedbackKind.Failure);
            }

            return;
        }

        _viewModel.SetDraftDisplayName(displayName);
        try {
            await _viewModel.SaveAsync();
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_save_success), UserFeedbackKind.Success);
            }
        } catch (Exception ex) {
            Log.Warn($"Could not save specimen: {ex.Message}");
            if (Activity != null) {
                Feedback.Send(Activity, GetString(Resource.String.specimen_save_failed), UserFeedbackKind.Failure);
            }
            RenderPhoto();
        }
    }

    private void CancelChanges() {
        if (_viewModel.IsBusy()) {
            return;
        }

        SpecimenDetailsMode mode = _viewModel.Mode;
        if (!_viewModel.CancelChanges()) {
            return;
        }

        if (mode == SpecimenDetailsMode.Create) {
            if (IsAdded) {
                ParentFragmentManager.PopBackStack();
            }
        }
    }

    private void Render() {
        if (_binding == null) {
            return;
        }

        if (_viewModel.Mode == SpecimenDetailsMode.Create) {
            _binding.specimen_name.Text = String.Empty;
            _binding.specimen_name_editor.Text = _viewModel.DraftDisplayName;
            _binding.specimen_species.Text = GetString(Resource.String.specimen_details_unknown_species);
        } else if (_viewModel.SpecimenEditor.Specimen != null) {
            _binding.specimen_name.Text = _viewModel.SpecimenEditor.Specimen.DisplayName;
            _binding.specimen_name_editor.Text = _viewModel.DraftDisplayName;
            _binding.specimen_species.Text = _viewModel.SpecimenEditor.Specimen.Species
                ?? GetString(Resource.String.specimen_details_unknown_species);
        }

        RenderPhoto();
        bool isWriting = _viewModel.Mode != SpecimenDetailsMode.Read;
        _binding.specimen_name.Visibility = isWriting ? ViewStates.Gone : ViewStates.Visible;
        _binding.specimen_name_editor.Visibility = isWriting ? ViewStates.Visible : ViewStates.Gone;
        _binding.specimen_photo_edit.Visibility = isWriting ? ViewStates.Visible : ViewStates.Gone;
        _binding.specimen_name_editor.Enabled = isWriting
            && _viewModel.OperationState == SpecimenDetailsOperationState.Idle;
        _binding.specimen_photo_edit.Enabled = isWriting
            && _viewModel.OperationState == SpecimenDetailsOperationState.Idle;
        _binding.operation_progress.Visibility = _viewModel.OperationState == SpecimenDetailsOperationState.Idle
            ? ViewStates.Gone
            : ViewStates.Visible;
    }

    private void RenderPhoto() {
        if (_binding == null) {
            return;
        }

        AndroidUri? photoUri = _viewModel.PhotoSession.GetDisplayUri();

        if (photoUri == null) {
            _binding.specimen_image.SetImageDrawable(null);
            _binding.specimen_photo_placeholder.Visibility = ViewStates.Visible;
        } else {
            _binding.specimen_image.SetImageURI(photoUri);
            _binding.specimen_photo_placeholder.Visibility = ViewStates.Gone;
        }
    }

    private void ShowPhotoFullscreen() {
        if (_viewModel.Mode != SpecimenDetailsMode.Read
            || _viewModel.OperationState != SpecimenDetailsOperationState.Idle
            || _photoFullscreenDialog != null) {
            return;
        }

        AndroidUri? photoUri = _viewModel.PhotoSession.GetDisplayUri();
        if (photoUri == null || Context == null) {
            return;
        }

        Dialog dialog = new(Context);
        ImageView image = new(Context) {
            LayoutParameters = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MatchParent,
                ViewGroup.LayoutParams.MatchParent),
        };
        image.SetScaleType(ImageView.ScaleType.FitCenter);
        image.SetBackgroundColor(Color.Black);
        image.SetImageURI(photoUri);
        image.Click += (_, _) => dialog.Dismiss();

        dialog.SetContentView(image);
        dialog.SetCancelable(true);
        dialog.DismissEvent += (_, _) => {
            if (ReferenceEquals(_photoFullscreenDialog, dialog)) {
                _photoFullscreenDialog = null;
            }
        };

        _photoFullscreenDialog = dialog;
        dialog.Show();

        Window? window = dialog.Window;
        if (window == null) {
            return;
        }

        window.SetBackgroundDrawable(new ColorDrawable(Color.Black));
        window.AddFlags(WindowManagerFlags.Fullscreen);
        window.SetLayout(ViewGroup.LayoutParams.MatchParent, ViewGroup.LayoutParams.MatchParent);
    }

    private void ClosePhotoFullscreen() {
        Dialog? dialog = _photoFullscreenDialog;
        _photoFullscreenDialog = null;
        if (dialog != null) {
            dialog.Dismiss();
        }
    }

    private void HandleViewModelStateChanged() {
        if (_binding != null) {
            Render();
        }

        UpdateToolbar();
    }

    private void HandleSpecimenNameTextChanged(object? sender, Android.Text.TextChangedEventArgs args) {
        _viewModel.SetDraftDisplayName(args.Text?.ToString() ?? String.Empty);
    }

    private void UpdateToolbar() {
        if (Activity is MainActivity activity) {
            ToolbarMenuAction[] actions = Array.Empty<ToolbarMenuAction>();
            if (_viewModel.OperationState == SpecimenDetailsOperationState.Idle) {
                switch(_viewModel.Mode) {
                case SpecimenDetailsMode.Read:
                    actions = [
                        new ToolbarMenuAction((int)ItemId.Delete,
                                              GetString(Resource.String.toolbar_menu_action_delete),
                                              Android.Resource.Drawable.IcMenuDelete,
                                              DeleteSpecimen),
                        new ToolbarMenuAction((int)ItemId.Edit,
                                              GetString(Resource.String.toolbar_menu_action_edit),
                                              Android.Resource.Drawable.IcMenuEdit,
                                              EnterEditMode),
                    ];
                    break;
                case SpecimenDetailsMode.Create:
                case SpecimenDetailsMode.Edit:
                    actions = [
                        new ToolbarMenuAction((int)ItemId.Save,
                                              GetString(Resource.String.toolbar_menu_action_save),
                                              Android.Resource.Drawable.IcMenuSave,
                                              () => _ = SaveAsync()),
                        new ToolbarMenuAction((int)ItemId.Cancel,
                                              GetString(Resource.String.toolbar_menu_action_cancel),
                                              Android.Resource.Drawable.IcMenuCloseClearCancel,
                                              CancelChanges),
                    ];
                    break;
                default:
                    throw new InvalidOperationException("Unknown specimen details mode.");
                }
            }

            activity.ConfigureToolbar(new ToolbarConfiguration(GetToolbarTitleResource(_viewModel.Mode), true), actions);
        }
    }

    private static int GetToolbarTitleResource(SpecimenDetailsMode mode) {
        switch (mode) {
        case SpecimenDetailsMode.Read:   return Resource.String.specimen_detail_title_read;
        case SpecimenDetailsMode.Edit:   return Resource.String.specimen_detail_title_edit;
        case SpecimenDetailsMode.Create: return Resource.String.specimen_detail_title_create;
        default: throw new ArgumentOutOfRangeException(nameof(mode), mode, null);
        }
    }

    private SpecimenDetailsMode ReadModeArgument() {
        int? mode = Arguments?.GetInt(ModeArgument, (int)SpecimenDetailsMode.Read);
        if (mode == null || !Enum.IsDefined(typeof(SpecimenDetailsMode), mode)) {
            Log.Warn("SpecimenDetail Invalid mode value");
            mode = (int)SpecimenDetailsMode.Read;
        }

        return (SpecimenDetailsMode)mode;
    }

    private MainVerteId ReadIdArgument(string key) {
        int? id = Arguments?.GetInt(key);
        if (id == null) {
            throw new InvalidOperationException($"Invalid {key}.");
        }

        return new MainVerteId(id.Value);
    }
}
