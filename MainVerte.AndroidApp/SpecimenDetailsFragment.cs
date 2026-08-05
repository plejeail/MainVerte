using System;
using System.Threading.Tasks;
using Android.OS;
using Android.Views;
using AndroidX.Fragment.App;

using AlertDialog = Android.App.AlertDialog;

using MainVerte.Core;

namespace MainVerte.AndroidApp;


enum SpecimenDetailsMode { Read, Edit, Create, }

sealed class SpecimenDetailsFragment : Fragment
{
    private const string ModeArgument = "mode";
    private const string SpecimenIdArgument = "specimen_id";
    private const string CollectionIdArgument = "collection_id";

    private enum ItemId { Edit = 1, Save = 2, Cancel = 3, Delete = 4, }

    private Binding.fragment_specimen_details? _binding;
    private SpecimenDetail? _specimen;
    private bool _isBusy;

    private SpecimenDetailsMode _mode;
    private MainVerteId _newSpecimenCollectionId = MainVerteId.Invalid;

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
        base.OnCreate(savedInstanceState);

        _mode = ReadModeArgument();
        if (_mode == SpecimenDetailsMode.Create) {
            _newSpecimenCollectionId = ReadIdArgument(CollectionIdArgument);
        }
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
        if (_mode == SpecimenDetailsMode.Create) {
            Render();
            return;
        }

        _ = LoadSpecimenAsync(ReadIdArgument(SpecimenIdArgument));
    }

    public override void OnResume() {
        base.OnResume();
        UpdateToolbar();
    }

    public override void OnDestroyView() {
        _binding = null;

        base.OnDestroyView();
    }

    private async Task LoadSpecimenAsync(MainVerteId specimenId) {
        Require.NotNull(_binding);

        try {
            SpecimenDetail? specimen = await Services.Database.GetSpecimenAsync(specimenId);
            if (_binding!.specimen_name == null || specimen == null) {
                return;
            }

            _specimen = specimen;
            Render();
        } catch (Exception ex) {
            Log.Error(ex.ToString());
        }
    }

    private async Task DeleteSpecimenAsync() {
        if (_isBusy) {
            return;
        }

        SpecimenDetail? specimen = _specimen;
        if (specimen == null) {
            return;
        }

        SetBusy(true);
        try {
            bool deleted = await Services.Database.DeleteSpecimenAsync(specimen.Id);
            if (!deleted) {
                return;
            }

            _specimen = null;
            if (IsAdded) {
                ParentFragmentManager.PopBackStack();
            }
        } catch (Exception ex) {
            Log.Error(ex.ToString());
        } finally {
            SetBusy(false);
        }
    }

    private void DeleteSpecimen() {
        if (_isBusy || _specimen == null || Activity == null) {
            return;
        }

        AlertDialog.Builder builder = new(Activity);
        builder.SetTitle(Resource.String.specimen_detail_delete_confirmation_title);
        builder.SetMessage(Resource.String.specimen_detail_delete_confirmation_message);
        builder.SetNegativeButton(Resource.String.specimen_detail_delete_confirmation_no, (_, _) => { });
        builder.SetPositiveButton(Resource.String.specimen_detail_delete_confirmation_yes,
                                  (_, _) => _ = DeleteSpecimenAsync());
        builder.Show();
    }

    private void EnterEditMode() {
        if (_isBusy || _specimen == null) {
            return;
        }

        _mode = SpecimenDetailsMode.Edit;
        Render();
        UpdateToolbar();
    }

    private async Task SaveAsync() {
        if (_isBusy || _binding == null) {
            return;
        }

        string displayName = _binding.specimen_name_editor.Text?.Trim() ?? String.Empty;
        if (displayName.Length == 0) {
            _binding.specimen_name_editor?.Error = GetString(Resource.String.specimen_detail_name_mandatory);

            return;
        }

        SetBusy(true);
        try {
            if (_mode == SpecimenDetailsMode.Create) {
                MainVerteId collectionId = _newSpecimenCollectionId;
                if (collectionId == MainVerteId.Invalid) {
                    throw new InvalidOperationException("Missing collection identifier for specimen creation.");
                }

                MainVerteId id = await Services.Database.CreateSpecimenAsync(new SpecimenDetail(
                    MainVerteId.Invalid,
                    collectionId,
                    null,
                    null,
                    null,
                    displayName,
                    null,
                    null,
                    0,
                    0
                ));
                _mode = SpecimenDetailsMode.Read;
                _specimen = await Services.Database.GetSpecimenAsync(id);
            } else {
                SpecimenDetail? specimen = _specimen;
                if (specimen == null) {
                    throw new InvalidOperationException("Specimen has not been loaded.");
                }

                bool updated = await Services.Database.UpdateSpecimenAsync(specimen with {
                    DisplayName = displayName,
                });

                if (!updated) {
                    throw new InvalidOperationException("Specimen no longer exists.");
                }

                _specimen = await Services.Database.GetSpecimenAsync(specimen.Id);
                _mode = SpecimenDetailsMode.Read;
            }

            if (_binding.specimen_name == null) {
                return;
            }

            Render();
            UpdateToolbar();
        } catch (Exception ex) {
            Log.Error(ex.ToString());
        } finally {
            SetBusy(false);
        }
    }

    private void CancelChanges() {
        if (_isBusy) {
            return;
        }

        if (_mode == SpecimenDetailsMode.Create) {
            ParentFragmentManager.PopBackStack();
            return;
        }

        _mode = SpecimenDetailsMode.Read;
        Render();
        UpdateToolbar();
    }

    private void Render() {
        if (_binding == null) {
            return;
        }

        if (_mode == SpecimenDetailsMode.Create) {
            _binding.specimen_name.Text        = String.Empty;
            _binding.specimen_name_editor.Text = String.Empty;
            _binding.specimen_species.Text     = GetString(Resource.String.specimen_details_unknown_species);
            _binding.specimen_image.SetImageDrawable(null);
        } else if (_specimen != null) {
            _binding.specimen_name.Text        = _specimen.DisplayName;
            _binding.specimen_name_editor.Text = _specimen.DisplayName;
            _binding.specimen_species.Text     = _specimen.Species ?? GetString(Resource.String.specimen_details_unknown_species);
            if (String.IsNullOrEmpty(_specimen.PhotoUri)) {
                _binding.specimen_image.SetImageDrawable(null);
            } else {
                _binding.specimen_image.SetImageURI(Android.Net.Uri.Parse(_specimen.PhotoUri));
            }
        }

        bool isWriting = _mode != SpecimenDetailsMode.Read;
        _binding.specimen_name.Visibility        = isWriting ? ViewStates.Gone    : ViewStates.Visible;
        _binding.specimen_name_editor.Visibility = isWriting ? ViewStates.Visible : ViewStates.Gone;
        _binding.specimen_name_editor.Enabled = isWriting;
    }

    private void SetBusy(bool busy) {
        _isBusy = busy;

        if (_binding != null) {
            _binding.operation_progress.Visibility = busy ? ViewStates.Visible : ViewStates.Gone;
            _binding.specimen_name_editor.Enabled = !busy && _mode != SpecimenDetailsMode.Read;
        }

        UpdateToolbar();
    }

    private void UpdateToolbar() {
        if (Activity is MainActivity activity) {
            ToolbarMenuAction[] actions = Array.Empty<ToolbarMenuAction>();
            if (!_isBusy) {
                switch(_mode) {
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

            activity.ConfigureToolbar(new ToolbarConfiguration(GetToolbarTitleResource(_mode), true), actions);
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
            Log.Warn($"SpecimenDetail Invalid mode value");
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
