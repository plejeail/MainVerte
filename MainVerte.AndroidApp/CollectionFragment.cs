using System;
using System.Threading.Tasks;
using Android.OS;
using Android.Util;
using Android.Views;
using AndroidX.Fragment.App;
using AndroidX.RecyclerView.Widget;

using MainVerte.Core;

using Log = MainVerte.Core.Log;

namespace MainVerte.AndroidApp;


sealed class CollectionFragment : Fragment
{
    private Binding.fragment_collection? _binding;
    private SpecimenAdapter? _adapter;
    public readonly ToolbarConfiguration ToolbarConfiguration = new(Resource.String.collection_title, false);

    public override View OnCreateView(LayoutInflater inflater, ViewGroup? container, Bundle? savedInstanceState) {
        Require.NotNull(inflater);

        View? root = inflater.Inflate(Resource.Layout.fragment_collection, container, attachToRoot: false);
        if (root == null) {
            throw new InvalidOperationException("Failed to inflate a view of the fragment collection");
        }

        _binding = new Binding.fragment_collection(root);
        return root;
    }

    public override void OnViewCreated(View view, Bundle? savedInstanceState) {
        Require.NotNull(_binding);

        base.OnViewCreated(view, savedInstanceState);
        _binding!.specimen_grid.SetLayoutManager(new GridLayoutManager(RequireContext(), ComputeSpanCount()));
        _binding.add_specimen.Click += (_, _) => {
            if (Activity is MainActivity activity) {
                activity.ShowAddSpecimen();
            }
        };

        _adapter = new SpecimenAdapter(id => {
            ParentFragmentManager.BeginTransaction()
                                 .Replace(Resource.Id.main_fragment_container, SpecimenDetailsFragment.ForSpecimen(id))
                                 .AddToBackStack(null)
                                 .Commit();
        });

        _binding.specimen_grid.SetAdapter(_adapter);
    }

    public override void OnResume() {
        base.OnResume();

        if (Activity is MainActivity activity) {
            activity.ConfigureToolbar(ToolbarConfiguration);
        }

        _ = LoadSpecimensAsync();
    }

    private async Task LoadSpecimensAsync() {
        try {
            var specimens = await Services.Database.ListSpecimensAsync();

            if (_binding == null || _adapter == null) {
                return;
            }

            _adapter.SetItems(specimens);
        } catch (Exception ex) {
            Log.Error(ex.ToString());
        }
    }

    private int ComputeSpanCount() {
        DisplayMetrics? metrics = Resources.DisplayMetrics;
        if (metrics == null) {
            return 2;
        }

        float screenWidthDp = metrics.WidthPixels / metrics.Density;

        return Math.Max(2, (int)(screenWidthDp / 180));
    }

    public override void OnDestroyView() {
        _binding = null;
        _adapter = null;
        base.OnDestroyView();
    }
}

sealed class SpecimenViewHolder : RecyclerView.ViewHolder
{
    private readonly Binding.specimen_card _binding;
    public MainVerteId Id;

    public SpecimenViewHolder(View itemView, Action<MainVerteId> onClicked) : base(itemView) {
        _binding = new Binding.specimen_card(itemView);
        Id = MainVerteId.Invalid;

        itemView.Click += (_, _) => {
            if (Id != MainVerteId.Invalid) {
                onClicked(Id);
            }
        };
    }

    public void Bind(SpecimenSummary specimen) {
        Id = specimen.Id;
        _binding.specimen_name.Text = specimen.Name;
        _binding.specimen_species.Text = specimen.Species;

        Android.Net.Uri? photoUri = PhotoStorage.GetDisplayUri(specimen.PhotoUri);
        if (photoUri != null) {
            _binding.specimen_image.SetImageURI(photoUri);
        } else {
            _binding.specimen_image.SetImageDrawable(null);
        }
    }
}

sealed class SpecimenAdapter(Action<MainVerteId> onClicked) : RecyclerView.Adapter
{
    private SpecimenSummary[] _specimens = Array.Empty<SpecimenSummary>();
    private readonly Action<MainVerteId> _onClicked = onClicked;

    public void SetItems(SpecimenSummary[] specimens) {
        Require.NotNull(specimens);
        _specimens = specimens;
        NotifyDataSetChanged();
    }

    public override void OnBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        if (holder is not SpecimenViewHolder viewHolder) {
            throw new InvalidOperationException("Unexpected specimen view holder type.");
        }

        viewHolder.Bind(_specimens[position]);
    }

    public override RecyclerView.ViewHolder OnCreateViewHolder(ViewGroup parent, int viewType) {
        LayoutInflater? inflater = LayoutInflater.From(parent.Context);
        if (inflater == null) {
            throw new InvalidOperationException("Could not create a layout inflater.");
        }

        View? itemView = inflater.Inflate(Resource.Layout.specimen_card, parent, attachToRoot: false);
        if (itemView == null) {
            throw new InvalidOperationException("Could not inflate specimen card.");
        }

        return new SpecimenViewHolder(itemView, _onClicked);
    }

    public override int ItemCount => _specimens.Length;
}
