---
##  Usage

CBCTAlign is organized as five sequential steps shown in the left-hand sidebar.

### Step 1 — Load CBCT Volumes
Import the longitudinal acquisitions via **File → Open NIfTI** (or **Open DICOM**), one file per timepoint. Each loaded volume appears in a summary table showing its shape (voxels), spacing (mm), origin, and field of view. Volumes are labeled T0, T1, T2, … in acquisition order.

![Step 1 — Load CBCT](docs/images/imga1.png)

### Step 2 — Cephalometric Landmarks
Import the ALI_CBCT JSON markup files via **File → Load Landmarks JSON**, one per timepoint. CBCTAlign extracts the landmark coordinates and populates a color-coded table with the name, abbreviation, and (x, y, z) position in millimeters for each timepoint.

![Step 2 — Landmarks](docs/images/imga3.png)

### Step 3 — Rigid Registration
Select the reference anchor (default: ANS at T0) and click **Run Registration**. Each moving volume is initialized from the landmark displacement, then refined by maximizing Mattes Mutual Information over a 3-level multi-resolution pyramid (shrink factors 4, 2, 1). The results table reports convergence status, final Mutual Information, and registration time per timepoint.

![Step 3 — Registration](docs/images/imga5.png)

### Step 4 — Slice Extraction
Set the number of slices per orientation and the range (± mm) around the anchor, then click **Run Extraction**. CBCTAlign extracts axial, coronal, and sagittal slices centered on the reference landmark for every timepoint, then normalizes the field of view across timepoints. A summary reports the number of timepoints, slices per orientation, and total slices saved.

![Step 4 — Extraction](docs/images/imga6.png)

### Step 5 — Validation & Visualization
The synchronized grid viewer displays the aligned 2D slices (three orientations × N timepoints). Each orientation has an independent slice index, Window/Level controls adjust contrast, and any panel can be opened in a zoom dialog. The MCAGPC metric is computed per orientation to quantify alignment quality.

![Step 5 — Validation](docs/images/imga7.png)

---
##  Configuration

CBCTAlign reads two optional environment variables so that no machine-specific paths are hard-coded:

| Variable | Purpose | Default if unset |
|----------|---------|------------------|
| `CBCTALIGN_DATA_DIR` | Folder where file dialogs open by default (your CBCT data and results). | `~/CBCTAlign_data` |
| `ALI_CBCT_HOME` | Folder containing your ALI_CBCT install (only needed for in-app landmark detection). | `~/ALI_CBCT_home` |

```bash
echo 'export CBCTALIGN_DATA_DIR="$HOME/path/to/your/data"' >> ~/.bashrc
echo 'export ALI_CBCT_HOME="$HOME/path/to/your/ALI_CBCT"' >> ~/.bashrc
source ~/.bashrc
```

If unset, CBCTAlign still runs — dialogs simply open at the default locations.# CBCTAlign
