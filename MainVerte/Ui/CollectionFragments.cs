using Android.OS;
using Android.Views;
using AndroidX.Fragment.App;
using AndroidX.RecyclerView.Widget;

namespace MainVerte.Ui;

public sealed class CollectionFragment : Fragment
{
    private Binding.FragmentCollection? _binding;
    private readonly PlantAdapter _adapter = new();

    public override View OnCreateView(
        LayoutInflater inflater,
        ViewGroup? container,
        Bundle? savedInstanceState)
    {
        ArgumentNullException.ThrowIfNull(inflater);

        View root = inflater.Inflate(
            Resource.Layout.fragment_collection,
            container,
            attachToRoot: false);

        _binding = new Binding.FragmentCollection(root);

        return root;
    }

    public override void OnViewCreated(
        View view,
        Bundle? savedInstanceState)
    {
        base.OnViewCreated(view, savedInstanceState);

        Binding.FragmentCollection binding =
            _binding
            ?? throw new InvalidOperationException(
                "The fragment view has not been created.");

        binding.plant_grid.SetLayoutManager(
            new GridLayoutManager(
                RequireContext(),
                spanCount: 2));

        binding.plant_grid.SetAdapter(_adapter);

        binding.add_plant_button.Click += OnAddPlantClicked;
    }

    public override void OnResume()
    {
        base.OnResume();

        _adapter.SetPlants(
            PlantRepository.Instance.GetAll());
    }

    public override void OnDestroyView()
    {
        if (_binding is not null)
        {
            _binding.add_plant_button.Click -=
                OnAddPlantClicked;

            _binding.plant_grid.SetAdapter(null);
        }

        _binding = null;

        base.OnDestroyView();
    }

    private void OnAddPlantClicked(
        object? sender,
        EventArgs eventArgs)
    {
        MainActivity activity =
            Activity as MainActivity
            ?? throw new InvalidOperationException(
                "CollectionFragment requires MainActivity.");

        activity.OpenAddPlant();
    }
}
