using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Android.App;
using Android.Content;
using Android.Graphics;
using AndroidX.Core.Content;
using MainVerte.Core;
using AndroidUri = Android.Net.Uri;
using Path = System.IO.Path;

namespace MainVerte.AndroidApp;


static class BitmapLoader
{
    public static Bitmap? LoadThumbnail(Context? context, Android.Net.Uri uri, int requestedWidth, int requestedHeight) {
        if (context == null) {
            return null;
        }

        BitmapFactory.Options options = new() {
            InJustDecodeBounds = true,
        };

        using (Stream? stream = context.ContentResolver?.OpenInputStream(uri)) {
            if (stream == null) {
                return null;
            }

            BitmapFactory.DecodeStream(stream, null, options);
        }

        options.InSampleSize = CalculateInSampleSize(options,
                                                     requestedWidth,
                                                     requestedHeight);
        options.InJustDecodeBounds = false;
        using (Stream? stream = context.ContentResolver?.OpenInputStream(uri)) {
            if (stream == null) {
                return null;
            }

            return BitmapFactory.DecodeStream(stream, null, options);
        }
    }

    public static int DpToPx(Context context, float dp) {
        float density = context.Resources?.DisplayMetrics?.Density ?? 1f;
        return (int)MathF.Round(dp * density);
    }


    private static int CalculateInSampleSize(BitmapFactory.Options options, int requestedWidth, int requestedHeight) {
        int height = options.OutHeight;
        int width = options.OutWidth;

        int inSampleSize = 1;

        if (height > requestedHeight || width > requestedWidth) {
            int halfHeight = height / 2;
            int halfWidth = width / 2;

            while (halfHeight / inSampleSize >= requestedHeight &&
                   halfWidth  / inSampleSize >= requestedWidth) {
                inSampleSize *= 2;
            }
        }

        return inSampleSize;
    }

}

static class PhotoStorage
{
    private const string PhotosDirectoryName     = "photos";
    private const string PendingDirectoryName    = "pending";
    private const string SpecimensDirectoryName  = "specimens";
    private const string ProviderFinalPathName  = "photo_specimens";
    private const string LegacyProviderPathName = "photos";
    private const string ProviderAuthoritySuffix = ".fileprovider";

    private static string GetFilesDirectory() {
        Java.IO.File? filesDirectory = Application.Context.FilesDir;
        if (filesDirectory == null || String.IsNullOrEmpty(filesDirectory.AbsolutePath)) {
            throw new InvalidOperationException("The application files directory is unavailable.");
        }

        return filesDirectory.AbsolutePath;
    }

    private static string GetPhotosDirectory() {
        return Path.Combine(GetFilesDirectory(), PhotosDirectoryName);
    }

    public static string GetPendingDirectory() {
        return Path.Combine(GetPhotosDirectory(), PendingDirectoryName);
    }

    private static string GetSpecimensDirectory() {
        return Path.Combine(GetPhotosDirectory(), SpecimensDirectoryName);
    }

    private static string GetProviderAuthority() {
        return Application.Context.PackageName + ProviderAuthoritySuffix;
    }

    public static void CleanupPendingFiles() {
        string pendingDirectory = GetPendingDirectory();
        if (!Directory.Exists(pendingDirectory)) {
            try {
                Directory.CreateDirectory(pendingDirectory);
            } catch (Exception ex) {
                Log.Warn("Photos unavailable, failed to create pending directory: " + ex.Message);
            }

            return;
        }

        string[] files = Directory.GetFiles(pendingDirectory);
        foreach (string file in files) {
            TryDelete(file);
        }
    }

    public static string CreatePendingFile() {
        return CreateUniqueFile(GetPendingDirectory());
    }

    public static string CreateSpecimenFile() {
        string directory = GetSpecimensDirectory();
        Directory.CreateDirectory(directory);

        while (true) {
            string path = Path.Combine(directory, Guid.NewGuid().ToString("N") + ".jpg");
            if (!File.Exists(path)) {
                return path;
            }
        }
    }

    private static string CreateUniqueFile(string directory) {
        Directory.CreateDirectory(directory);

        while (true) {
            string path = Path.Combine(directory, Guid.NewGuid().ToString("N") + ".jpg");
            try {
                using FileStream stream = new(path, FileMode.CreateNew, FileAccess.Write, FileShare.None);
                return path;
            } catch (IOException) when (File.Exists(path)) {
                // A GUID collision is exceptionally unlikely, but the helper must remain safe.
            }
        }
    }

    /// <summary> Handle the case where a photo file already exists while it shouldn't. </summary>
    /// <param name="path">The path to handle</param>
    /// <remarks> This is an error path. It always fails in Debug mode. </remarks>
    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void HandleExistingFileProblem(string path) {
        Ensure.True(false, "The photo path already exists.");
        Log.Warn($"The photo path {path} already exists, removing the file.");
        try {
            File.Delete(path);
        } catch (IOException ex) {
            Log.Warn("Failed to delete the photo path: " + ex.Message);
        }
    }

    public static void CopyToFinal(string pendingPath, string finalPath) {
        EnsurePathInDirectory(pendingPath, GetPendingDirectory());
        EnsurePathInDirectory(finalPath, GetSpecimensDirectory());
        if (File.Exists(finalPath)) {
            HandleExistingFileProblem(finalPath);
        }

        File.Move(pendingPath, finalPath);
    }

    public static void MoveFinalToPending(string finalPath, string pendingPath) {
        EnsurePathInDirectory(finalPath, GetSpecimensDirectory());
        EnsurePathInDirectory(pendingPath, GetPendingDirectory());
        if (File.Exists(pendingPath)) {
            HandleExistingFileProblem(pendingPath);
        }

        File.Move(finalPath, pendingPath);
    }

    public static async Task CopyUriToFileAsync(ContentResolver resolver,
                                                AndroidUri sourceUri,
                                                string destinationPath,
                                                CancellationToken cancellationToken) {
        Ensure.NotNull(resolver);
        Ensure.NotNull(sourceUri);
        EnsurePathInDirectory(destinationPath, GetPendingDirectory());

        await using Stream? input = resolver.OpenInputStream(sourceUri);
        if (input == null) {
            Log.Error("The selected photo cannot be opened.");
            return;
        }

        const int outputBufSize = 81920;
        await using FileStream output = new(destinationPath, FileMode.Create, FileAccess.Write,
                                            FileShare.None, outputBufSize, useAsync: true);
        byte[] buffer = new byte[outputBufSize];
        while (true) {
            int read = await input.ReadAsync(buffer.AsMemory(0, buffer.Length), cancellationToken);
            if (read == 0) {
                break;
            }

            await output.WriteAsync(buffer.AsMemory(0, read), cancellationToken);
        }

        await output.FlushAsync(cancellationToken);
    }

    public static AndroidUri GetContentUri(string path) {
        EnsurePathInDirectory(path, GetPhotosDirectory());

        Java.IO.File file = new(path);
        AndroidUri? uri = FileProvider.GetUriForFile(Application.Context, GetProviderAuthority(), file);
        if (uri == null) {
            throw new InvalidOperationException("The application file provider did not return a URI.");
        }

        return uri;
    }

    public static AndroidUri? GetDisplayUri(string? photoUri) {
        if (String.IsNullOrWhiteSpace(photoUri)) {
            return null;
        }

        try {
            string? ownedPath;
            if (TryGetOwnedFinalPath(photoUri, out ownedPath)) {
                if (!File.Exists(ownedPath)) {
                    return null;
                }

                return GetContentUri(ownedPath);
            }

            return AndroidUri.Parse(photoUri);
        } catch (Exception ex) {
            Log.Warn($"Could not resolve photo URI: {ex.Message}");
            return null;
        }
    }

    public static bool TryGetOwnedFinalPath(string? photoUri, out string? path) {
        path = null;
        if (String.IsNullOrWhiteSpace(photoUri)) {
            return false;
        }

        if (Path.IsPathRooted(photoUri)) {
            string absolutePath = Path.GetFullPath(photoUri);
            if (!IsPathInDirectory(absolutePath, GetSpecimensDirectory())) {
                return false;
            }

            path = absolutePath;
            return true;
        }

        AndroidUri? uri;
        try {
            uri = AndroidUri.Parse(photoUri);
        } catch (Exception) {
            return false;
        }

        if (uri == null
            || !String.Equals(uri.Scheme, "content", StringComparison.OrdinalIgnoreCase)
            || !String.Equals(uri.Authority, GetProviderAuthority(), StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        string? encodedPath = uri.Path;
        string? relativePath = null;
        const string providerPrefix = "/" + ProviderFinalPathName + "/";
        if (!String.IsNullOrEmpty(encodedPath)
            && encodedPath.StartsWith(providerPrefix, StringComparison.Ordinal)) {
            relativePath = Uri.UnescapeDataString(encodedPath[providerPrefix.Length..]);
        }

        const string legacyProviderPrefix = "/" + LegacyProviderPathName + "/" + SpecimensDirectoryName + "/";
        if (relativePath == null
            && !String.IsNullOrEmpty(encodedPath)
            && encodedPath.StartsWith(legacyProviderPrefix, StringComparison.Ordinal)) {
            relativePath = Uri.UnescapeDataString(encodedPath[legacyProviderPrefix.Length..]);
        }

        if (relativePath == null) {
            return false;
        }

        relativePath = relativePath.Replace('/', Path.DirectorySeparatorChar);
        string candidate = Path.GetFullPath(Path.Combine(GetSpecimensDirectory(), relativePath));
        if (!IsPathInDirectory(candidate, GetSpecimensDirectory())) {
            return false;
        }

        path = candidate;
        return true;
    }

    public static bool TryDelete(string? path) {
        if (String.IsNullOrEmpty(path)) {
            return true;
        }

        string absolutePath;
        try {
            absolutePath = Path.GetFullPath(path);
        } catch (Exception) {
            return false;
        }

        if (!IsPathInDirectory(absolutePath, GetPhotosDirectory())) {
            Log.Warn($"Refusing to delete photo outside the application photo directory: {path}");
            return false;
        }

        try {
            if (File.Exists(absolutePath)) {
                File.Delete(absolutePath);
            }

            return true;
        } catch (Exception ex) {
            Log.Warn($"Could not delete photo '{absolutePath}': {ex.Message}");
            return false;
        }
    }

    public static void DeleteOwnedFinalPhoto(string? photoUri) {
        string? path;
        if (TryGetOwnedFinalPath(photoUri, out path)) {
            TryDelete(path);
        }
    }

    private static void EnsurePathInDirectory(string path, string directory) {
        Ensure.True(!String.IsNullOrEmpty(path) && IsPathInDirectory(path, directory),
                    "The photo path is outside the application photo directories.");
    }

    private static bool IsPathInDirectory(string path, string directory) {
        string absolutePath = Path.GetFullPath(path);
        string absoluteDirectory = Path.GetFullPath(directory);
        string directoryPrefix = absoluteDirectory.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal)
            ? absoluteDirectory
            : absoluteDirectory + Path.DirectorySeparatorChar;

        return absolutePath.StartsWith(directoryPrefix, StringComparison.OrdinalIgnoreCase);
    }
}

enum PhotoCaptureResult
{
    NoCapture,
    Cancelled,
    Failed,
    Succeeded,
}

sealed class PhotoEditSession
{
    private enum PhotoDraftState
    {
        KeepExisting,
        ReplaceWithTemporaryFile,
        Remove,
        Empty,
    }

    private string? _originalPhotoUri;
    private PhotoDraftState _photoDraftState = PhotoDraftState.Empty;
    private string? _photoDraftTemporaryPath;
    private string? _preparedFinalPhotoPath;
    private string? _cameraCapturePath;
    private CancellationTokenSource? _photoCopyCancellation;

    public bool IsCopying() {
        return _photoCopyCancellation != null;
    }

    public bool HasPhoto() {
        if (_photoDraftState == PhotoDraftState.KeepExisting) {
            return !String.IsNullOrEmpty(_originalPhotoUri);
        }

        return _photoDraftState == PhotoDraftState.ReplaceWithTemporaryFile
            && _photoDraftTemporaryPath != null
            && File.Exists(_photoDraftTemporaryPath);
    }

    public void SetOriginalPhoto(string? photoUri) {
        _originalPhotoUri = photoUri;
        ResetToOriginal();
    }

    public void ResetToOriginal() {
        if (String.IsNullOrEmpty(_originalPhotoUri)) {
            _photoDraftState = PhotoDraftState.Empty;
        } else {
            _photoDraftState = PhotoDraftState.KeepExisting;
        }
    }

    public async Task ImportGalleryPhotoAsync(ContentResolver resolver, AndroidUri sourceUri) {
        if (IsCopying()) {
            return;
        }

        CancellationTokenSource cancellation = new();
        string? pendingPath = null;
        _photoCopyCancellation = cancellation;
        try {
            string createdPendingPath = PhotoStorage.CreatePendingFile();
            pendingPath = createdPendingPath;
            await PhotoStorage.CopyUriToFileAsync(resolver, sourceUri, createdPendingPath, cancellation.Token);
            if (cancellation.IsCancellationRequested) {
                return;
            }

            AdoptTemporaryPhoto(pendingPath);
            pendingPath = null;
        } finally {
            if (pendingPath != null) {
                PhotoStorage.TryDelete(pendingPath);
            }

            if (ReferenceEquals(_photoCopyCancellation, cancellation)) {
                _photoCopyCancellation = null;
            }

            cancellation.Dispose();
        }
    }

    public AndroidUri BeginCameraCapture() {
        if (_cameraCapturePath != null) {
            throw new InvalidOperationException("A camera capture is already in progress.");
        }

        string pendingPath = PhotoStorage.CreatePendingFile();
        try {
            AndroidUri outputUri = PhotoStorage.GetContentUri(pendingPath);
            _cameraCapturePath = pendingPath;
            return outputUri;
        } catch {
            PhotoStorage.TryDelete(pendingPath);
            throw;
        }
    }

    public void CancelCameraCapture() {
        PhotoStorage.TryDelete(_cameraCapturePath);
        _cameraCapturePath = null;
    }

    public PhotoCaptureResult CompleteCameraCapture(bool succeeded) {
        string? capturePath = _cameraCapturePath;
        _cameraCapturePath = null;
        if (capturePath == null) {
            return PhotoCaptureResult.NoCapture;
        }

        if (!succeeded) {
            PhotoStorage.TryDelete(capturePath);
            return PhotoCaptureResult.Cancelled;
        }

        try {
            FileInfo info = new(capturePath);
            if (!info.Exists || info.Length == 0) {
                PhotoStorage.TryDelete(capturePath);
                return PhotoCaptureResult.Failed;
            }

            AdoptTemporaryPhoto(capturePath);
            return PhotoCaptureResult.Succeeded;
        } catch (Exception ex) {
            PhotoStorage.TryDelete(capturePath);
            Log.Warn($"Could not use captured photo: {ex.Message}");
            return PhotoCaptureResult.Failed;
        }
    }

    public void CancelChanges() {
        CleanupUncommittedPhotoFiles();
        ResetToOriginal();
    }

    public void RemovePhoto(bool editingExistingSpecimen) {
        PhotoStorage.TryDelete(_photoDraftTemporaryPath);
        _photoDraftTemporaryPath = null;
        _photoDraftState = editingExistingSpecimen
            ? PhotoDraftState.Remove
            : PhotoDraftState.Empty;
    }

    public string? PrepareForSave() {
        switch (_photoDraftState) {
        case PhotoDraftState.KeepExisting:
            return _originalPhotoUri;
        case PhotoDraftState.Remove:
        case PhotoDraftState.Empty:
            return null;
        case PhotoDraftState.ReplaceWithTemporaryFile:
            string? temporaryPath = _photoDraftTemporaryPath;
            if (String.IsNullOrEmpty(temporaryPath) || !File.Exists(temporaryPath)) {
                throw new InvalidOperationException("The temporary photo is unavailable.");
            }

            string finalPath = PhotoStorage.CreateSpecimenFile();
            _preparedFinalPhotoPath = finalPath;
            PhotoStorage.CopyToFinal(temporaryPath, finalPath);
            return PhotoStorage.GetContentUri(finalPath).ToString();
        default:
            throw new ArgumentOutOfRangeException();
        }
    }

    public void Commit(string? savedPhotoUri, string? replacedPhotoUri) {
        if (!String.Equals(replacedPhotoUri, savedPhotoUri, StringComparison.Ordinal)) {
            PhotoStorage.DeleteOwnedFinalPhoto(replacedPhotoUri);
        }

        _originalPhotoUri = savedPhotoUri;
        _photoDraftState = String.IsNullOrEmpty(savedPhotoUri)
            ? PhotoDraftState.Empty
            : PhotoDraftState.KeepExisting;
        _photoDraftTemporaryPath = null;
        _preparedFinalPhotoPath = null;
    }

    public void RestorePreparedPhotoAfterFailure() {
        string? finalPath = _preparedFinalPhotoPath;
        if (finalPath == null) {
            return;
        }

        try {
            string? pendingPath = _photoDraftTemporaryPath;
            if (File.Exists(finalPath)
                && pendingPath != null
                && !File.Exists(pendingPath)) {
                PhotoStorage.MoveFinalToPending(finalPath, pendingPath);
            } else {
                PhotoStorage.TryDelete(finalPath);
            }
        } catch (Exception ex) {
            Log.Warn($"Could not restore temporary photo after save failure: {ex.Message}");
            PhotoStorage.TryDelete(finalPath);
            _photoDraftTemporaryPath = null;
            ResetToOriginal();
        }

        _preparedFinalPhotoPath = null;
    }

    public void CleanupUncommittedPhotoFiles() {
        _photoCopyCancellation?.Cancel();
        PhotoStorage.TryDelete(_cameraCapturePath);
        PhotoStorage.TryDelete(_photoDraftTemporaryPath);
        PhotoStorage.TryDelete(_preparedFinalPhotoPath);
        _cameraCapturePath = null;
        _photoDraftTemporaryPath = null;
        _preparedFinalPhotoPath = null;
        _photoDraftState = String.IsNullOrEmpty(_originalPhotoUri)
                         ? PhotoDraftState.Empty
                         : PhotoDraftState.KeepExisting;
    }

    public AndroidUri? GetDisplayUri() {
        AndroidUri? photoUri = null;
        try {
            switch (_photoDraftState) {
            case PhotoDraftState.KeepExisting:
                photoUri = PhotoStorage.GetDisplayUri(_originalPhotoUri);
                break;
            case PhotoDraftState.ReplaceWithTemporaryFile:
                if (_photoDraftTemporaryPath != null && File.Exists(_photoDraftTemporaryPath)) {
                    photoUri = PhotoStorage.GetContentUri(_photoDraftTemporaryPath);
                }

                break;
            case PhotoDraftState.Remove:
            case PhotoDraftState.Empty:
                break;
            default:
                throw new ArgumentOutOfRangeException();
            }
        } catch (Exception ex) {
            Log.Warn($"Could not resolve specimen photo: {ex.Message}");
            photoUri = null;
        }

        return photoUri;
    }

    private void AdoptTemporaryPhoto(string path) {
        string? previousPath = _photoDraftTemporaryPath;
        if (!String.Equals(previousPath, path, StringComparison.Ordinal)) {
            PhotoStorage.TryDelete(previousPath);
        }

        _photoDraftTemporaryPath = path;
        _photoDraftState = PhotoDraftState.ReplaceWithTemporaryFile;
    }
}
