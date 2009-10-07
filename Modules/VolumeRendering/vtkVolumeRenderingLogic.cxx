#include "vtkVolumeRenderingLogic.h"
#include "vtkObjectFactory.h"
#include "vtkObject.h"
#include "vtkVolumeProperty.h"
#include "vtkImageData.h"
#include "vtkPointData.h"
#include "vtkMatrix4x4.h"

#include "vtkMRMLVolumeRenderingParametersNode.h"
#include "vtkMRMLVolumeRenderingScenarioNode.h"
#include "vtkMRMLTransformNode.h"

#include "vtkSlicerVolumeTextureMapper3D.h"
#include "vtkSlicerFixedPointVolumeRayCastMapper.h"
#include "vtkSlicerGPURayCastVolumeTextureMapper3D.h"
#include "vtkSlicerGPURayCastVolumeMapper.h"
#include "vtkCudaVolumeMapper.h"
#include "vtkImageGradientMagnitude.h"

#include "vtkKWHistogramSet.h"
#include "vtkKWHistogram.h"

bool vtkVolumeRenderingLogic::First = true;

vtkVolumeRenderingLogic::vtkVolumeRenderingLogic(void)
{
  //create instances of mappers
  this->MapperTexture = vtkSlicerVolumeTextureMapper3D::New();

  this->MapperCUDARaycast = vtkCudaVolumeMapper::New();

  this->MapperGPURaycast = vtkSlicerGPURayCastVolumeTextureMapper3D::New();

  this->MapperGPURaycastII = vtkSlicerGPURayCastVolumeMapper::New();

  this->MapperRaycast=vtkSlicerFixedPointVolumeRayCastMapper::New();

  //create instance of the actor
  this->Volume = vtkVolume::New();

  this->Histograms = vtkKWHistogramSet::New();

  this->GUICallback = NULL;
}

vtkVolumeRenderingLogic::~vtkVolumeRenderingLogic(void)
{
  //delete instances
  if (this->MapperTexture)
  {
    this->MapperTexture->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperTexture->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperTexture->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperTexture->Delete();
    this->MapperTexture = NULL;
  }
  if (this->MapperCUDARaycast)
  {
    this->MapperCUDARaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperCUDARaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperCUDARaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperCUDARaycast->Delete();
    this->MapperCUDARaycast = NULL;
  }
  if (this->MapperGPURaycast)
  {
    this->MapperGPURaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperGPURaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperGPURaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperGPURaycast->Delete();
    this->MapperGPURaycast = NULL;
  }
  if (this->MapperGPURaycastII)
  {
    this->MapperGPURaycastII->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperGPURaycastII->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperGPURaycastII->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);

    this->MapperGPURaycastII->Delete();
    this->MapperGPURaycastII = NULL;
  }
  if (this->MapperRaycast)
  {
    this->MapperRaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperRaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperRaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperRaycast->RemoveObservers(vtkCommand::ProgressEvent, this->GUICallback);
    this->MapperRaycast->Delete();
    this->MapperRaycast = NULL;
  }
  if (this->Volume)
  {
    this->Volume->Delete();
    this->Volume = NULL;
  }
  if(this->Histograms != NULL)
  {
    this->Histograms->RemoveAllHistograms();
    this->Histograms->Delete();
    this->Histograms = NULL;
  }
}

vtkVolumeRenderingLogic* vtkVolumeRenderingLogic::New()
{
 // First try to create the object from the vtkObjectFactory
  vtkObject* ret = vtkObjectFactory::CreateInstance("vtkVolumeRenderingLogic");
  if(ret)
    {
      return (vtkVolumeRenderingLogic*)ret;
    }
  // If the factory was unable to create the object, then create it here.
  return new vtkVolumeRenderingLogic;
}

void vtkVolumeRenderingLogic::PrintSelf(std::ostream &os, vtkIndent indent)
{
    os<<indent<<"Print logic"<<endl;
}

void vtkVolumeRenderingLogic::SetMRMLScene(vtkMRMLScene *scene)
{
  vtkSlicerModuleLogic::SetMRMLScene(scene);
  this->RegisterNodes();
}

void vtkVolumeRenderingLogic::RegisterNodes()
{
  if (this->MRMLScene && this->First)
  {
    // :NOTE: 20050513 tgl: Guard this so it is only registered once.
    vtkMRMLVolumeRenderingScenarioNode *vrsNode = vtkMRMLVolumeRenderingScenarioNode::New();
    this->MRMLScene->RegisterNodeClass(vrsNode);
    vrsNode->Delete();

    vtkMRMLVolumeRenderingParametersNode *vrpNode = vtkMRMLVolumeRenderingParametersNode::New();
    this->MRMLScene->RegisterNodeClass(vrpNode);
    vrpNode->Delete();

    this->First = false;
  }
}

void vtkVolumeRenderingLogic::SetGUICallbackCommand(vtkCommand* callback)
{
  this->MapperTexture->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, callback);
  this->MapperTexture->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, callback);
  this->MapperTexture->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, callback);

  //cpu ray casting
  this->MapperRaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, callback);
  this->MapperRaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, callback);
  this->MapperRaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, callback);
  this->MapperRaycast->AddObserver(vtkCommand::ProgressEvent,callback);

  //hook up the cuda mapper
  this->MapperCUDARaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, callback);
  this->MapperCUDARaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, callback);
  this->MapperCUDARaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, callback);

  //hook up the gpu mapper
  this->MapperGPURaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, callback);
  this->MapperGPURaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, callback);
  this->MapperGPURaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, callback);

  this->MapperGPURaycastII->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, callback);

  this->GUICallback = callback;
}

void vtkVolumeRenderingLogic::Reset()
{
  //delete instances
  if (this->MapperTexture)
  {
    this->MapperTexture->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperTexture->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperTexture->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperTexture->Delete();
    this->MapperTexture = NULL;
  }
  if (this->MapperCUDARaycast)
  {
    this->MapperCUDARaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperCUDARaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperCUDARaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperCUDARaycast->Delete();
    this->MapperCUDARaycast = NULL;
  }
  if (this->MapperGPURaycast)
  {
    this->MapperGPURaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperGPURaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperGPURaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperGPURaycast->Delete();
    this->MapperGPURaycast = NULL;
  }
  if (this->MapperGPURaycastII)
  {
    this->MapperGPURaycastII->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperGPURaycastII->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperGPURaycastII->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);

    this->MapperGPURaycastII->Delete();
    this->MapperGPURaycastII = NULL;
  }
  if (this->MapperRaycast)
  {
    this->MapperRaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
    this->MapperRaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
    this->MapperRaycast->RemoveObservers(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
    this->MapperRaycast->RemoveObservers(vtkCommand::ProgressEvent, this->GUICallback);
    this->MapperRaycast->Delete();
    this->MapperRaycast = NULL;
  }
  if (this->Volume)
  {
    this->Volume->Delete();
    this->Volume = NULL;
  }
  if(this->Histograms != NULL)
  {
    this->Histograms->RemoveAllHistograms();
    this->Histograms->Delete();
    this->Histograms = NULL;
  }

  //create instances of mappers
  this->MapperTexture = vtkSlicerVolumeTextureMapper3D::New();

  this->MapperCUDARaycast = vtkCudaVolumeMapper::New();

  this->MapperGPURaycast = vtkSlicerGPURayCastVolumeTextureMapper3D::New();

  this->MapperGPURaycastII = vtkSlicerGPURayCastVolumeMapper::New();

  this->MapperRaycast=vtkSlicerFixedPointVolumeRayCastMapper::New();

  //create instance of the actor
  this->Volume = vtkVolume::New();

  this->Histograms = vtkKWHistogramSet::New();

  this->MapperTexture->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
  this->MapperTexture->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
  this->MapperTexture->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);

  //cpu ray casting
  this->MapperRaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
  this->MapperRaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
  this->MapperRaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);
  this->MapperRaycast->AddObserver(vtkCommand::ProgressEvent, this->GUICallback);

  //hook up the cuda mapper
  this->MapperCUDARaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
  this->MapperCUDARaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
  this->MapperCUDARaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);

  //hook up the gpu mapper
  this->MapperGPURaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsStartEvent, this->GUICallback);
  this->MapperGPURaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
  this->MapperGPURaycast->AddObserver(vtkCommand::VolumeMapperComputeGradientsEndEvent, this->GUICallback);

  this->MapperGPURaycastII->AddObserver(vtkCommand::VolumeMapperComputeGradientsProgressEvent, this->GUICallback);
}

vtkMRMLVolumeRenderingParametersNode* vtkVolumeRenderingLogic::CreateParametersNode()
{
  vtkMRMLVolumeRenderingParametersNode *node = NULL;

  if (this->MRMLScene)
  {
    node = vtkMRMLVolumeRenderingParametersNode::New();
    this->MRMLScene->AddNode(node);
    node->Delete();
  }

  return node;
}

vtkMRMLVolumeRenderingScenarioNode* vtkVolumeRenderingLogic::CreateScenarioNode()
{
  vtkMRMLVolumeRenderingScenarioNode *node = NULL;

  if (this->MRMLScene)
  {
    node = vtkMRMLVolumeRenderingScenarioNode::New();
    this->MRMLScene->AddNode(node);
    node->Delete();
  }

  return node;
}

void vtkVolumeRenderingLogic::SetupHistograms(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  vtkImageData *input = vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetImageData();

  //-----------------------------------------
  //  remove old histogram
  //-----------------------------------------
  if(this->Histograms != NULL)
  {
    this->Histograms->RemoveAllHistograms();
    this->Histograms->Delete();
    this->Histograms = vtkKWHistogramSet::New();
  }

  //setup histograms
  this->Histograms->AddHistograms(input->GetPointData()->GetScalars());

  //gradient histogram
  vtkImageGradientMagnitude *grad = vtkImageGradientMagnitude::New();
  grad->SetDimensionality(3);
  grad->SetInput(input);
  grad->Update();

  vtkKWHistogram *gradHisto = vtkKWHistogram::New();
  gradHisto->BuildHistogram(grad->GetOutput()->GetPointData()->GetScalars(), 0);
  this->Histograms->AddHistogram(gradHisto, "0gradient");

  grad->Delete();
  gradHisto->Delete();
}

void vtkVolumeRenderingLogic::SetupVolumePropertyFromImageData(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  vtkImageData *input = vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetImageData();
  vtkVolumeProperty *prop = vspNode->GetVolumePropertyNode()->GetVolumeProperty();

  //update scalar range
  vtkColorTransferFunction *functionColor = prop->GetRGBTransferFunction();

  double rangeNew[2];
  input->GetPointData()->GetScalars()->GetRange(rangeNew);
  functionColor->AdjustRange(rangeNew);

  vtkPiecewiseFunction *functionOpacity = prop->GetScalarOpacity();
  functionOpacity->AdjustRange(rangeNew);

  rangeNew[1] = (rangeNew[1] - rangeNew[0])*0.25;
  rangeNew[0] = 0;

  functionOpacity = prop->GetGradientOpacity();
  functionOpacity->RemovePoint(255);//Remove the standard value
  functionOpacity->AdjustRange(rangeNew);

  this->SetupHistograms(vspNode);

  //add points into transfer functions
  vtkKWHistogram *histogram = this->Histograms->GetHistogramWithName("0");

  double totalOccurance = histogram->GetTotalOccurence();
  double thresholdLow = totalOccurance * 0.2;
  double thresholdHigh = totalOccurance * 0.8;
  double range[2];

  histogram->GetRange(range);

  double thresholdLowIndex = range[0];
  double sumLowIndex = 0;
  double thresholdHighIndex = range[0];
  double sumHighIndex = 0;

  //calculate distance
  double bin_width = (range[1] == range[0] ? 1 : (range[1] - range[0])/(double)histogram->GetNumberOfBins());

  while(sumLowIndex < thresholdLow)
  {
    sumLowIndex += histogram->GetOccurenceAtValue(thresholdLowIndex);
    thresholdLowIndex += bin_width;
  }

  while(sumHighIndex < thresholdHigh)
  {
    sumHighIndex += histogram->GetOccurenceAtValue(thresholdHighIndex);
    thresholdHighIndex += bin_width;
  }

  prop->SetInterpolationTypeToLinear();
  vtkPiecewiseFunction *opacity = prop->GetScalarOpacity();

  opacity->RemoveAllPoints();
  opacity->AddPoint(range[0], 0.0);
  opacity->AddPoint(thresholdLowIndex, 0.0);
  opacity->AddPoint(thresholdHighIndex, 0.2);
  opacity->AddPoint(range[1], 0.2);

  vtkColorTransferFunction *colorTransfer = prop->GetRGBTransferFunction();

  colorTransfer->RemoveAllPoints();
  colorTransfer->AddRGBPoint(range[0], 0.3, 0.3, 1.0);
  colorTransfer->AddRGBPoint(thresholdLowIndex, 0.3, 0.3, 1.0);
  colorTransfer->AddRGBPoint(thresholdLowIndex + 0.5 * (thresholdHighIndex - thresholdLowIndex), 0.3, 1.0, 0.3);
  colorTransfer->AddRGBPoint(thresholdHighIndex, 1.0, 0.3, 0.3);
  colorTransfer->AddRGBPoint(range[1], 1.0, 0.3, 0.3);

  prop->ShadeOn();
  prop->SetAmbient(0.20);
  prop->SetDiffuse(0.80);
  prop->SetSpecular(0.50);
  prop->SetSpecularPower(40);
}

void vtkVolumeRenderingLogic::ComputeInternalVolumeSize(int index)
{
  switch(index)
  {
  case 0://128M
    this->MapperGPURaycast->SetInternalVolumeSize(200);
    this->MapperGPURaycastII->SetInternalVolumeSize(200);
    this->MapperTexture->SetInternalVolumeSize(128);//has to be power-of-two in this mapper
    break;
  case 1://256M
    this->MapperGPURaycast->SetInternalVolumeSize(256);//256^3
    this->MapperGPURaycastII->SetInternalVolumeSize(256);
    this->MapperTexture->SetInternalVolumeSize(256);
    break;
  case 2://512M
    this->MapperGPURaycast->SetInternalVolumeSize(320);
    this->MapperGPURaycastII->SetInternalVolumeSize(320);
    this->MapperTexture->SetInternalVolumeSize(256);
    break;
  case 3://1024M
    this->MapperGPURaycast->SetInternalVolumeSize(400);
    this->MapperGPURaycastII->SetInternalVolumeSize(400);
    this->MapperTexture->SetInternalVolumeSize(256);
    break;
  case 4://1.5G
    this->MapperGPURaycast->SetInternalVolumeSize(460);
    this->MapperGPURaycastII->SetInternalVolumeSize(460);
    this->MapperTexture->SetInternalVolumeSize(256);
    break;
  case 5://2.0G
    this->MapperGPURaycast->SetInternalVolumeSize(512);
    this->MapperGPURaycastII->SetInternalVolumeSize(512);
    this->MapperTexture->SetInternalVolumeSize(512);
    break;
  }
}

void vtkVolumeRenderingLogic::CalculateMatrix(vtkMRMLVolumeRenderingParametersNode *vspNode, vtkMatrix4x4 *output)
{
  //Update matrix
  //Check for NUll Pointer

  vtkMRMLTransformNode *tmp = vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetParentTransformNode();
  //check if we have a TransformNode
  if(tmp == NULL)
  {
    vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetIJKToRASMatrix(output);
    return;
  }

  //IJK to ras
  vtkMatrix4x4 *matrix = vtkMatrix4x4::New();
  vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetIJKToRASMatrix(matrix);

  // Parent transforms
  vtkMatrix4x4   *transform = vtkMatrix4x4::New();
  tmp->GetMatrixTransformToWorld(transform);

  //Transform world to ras
  vtkMatrix4x4::Multiply4x4(transform, matrix, output);

  matrix->Delete();
  transform->Delete();
}

void vtkVolumeRenderingLogic::SetExpectedFPS(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  int fps = vspNode->GetExpectedFPS();

  this->MapperTexture->SetFramerate(fps);
  this->MapperGPURaycast->SetFramerate(fps);
  this->MapperGPURaycastII->SetFramerate(fps);

  this->MapperCUDARaycast->SetIntendedFrameRate(fps);
}

void vtkVolumeRenderingLogic::SetGPUMemorySize(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  this->ComputeInternalVolumeSize(vspNode->GetGPUMemorySize());
}

void vtkVolumeRenderingLogic::SetCPURaycastParameters(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  if (vspNode->GetCPURaycastMode())
    this->MapperRaycast->SetBlendModeToMaximumIntensity();
  else
    this->MapperRaycast->SetBlendModeToComposite();
}

void vtkVolumeRenderingLogic::SetGPURaycastParameters(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  this->MapperGPURaycast->SetDepthPeelingThreshold(vspNode->GetDepthPeelingThreshold());
  this->MapperGPURaycast->SetICPEScale(vspNode->GetICPEScale());
  this->MapperGPURaycast->SetICPESmoothness(vspNode->GetICPESmoothness());
  this->MapperGPURaycast->SetTechnique(vspNode->GetGPURaycastTechnique());
}

void vtkVolumeRenderingLogic::EstimateSampleDistance(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  double *spacing = vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetSpacing();

  if (spacing)
  {
    double minSpace = spacing[0];
    double maxSpace = spacing[0];

    for(int i = 1; i < 3; i++)
    {
      if (spacing[i] > maxSpace)
        maxSpace = spacing[i];
      if (spacing[i] < minSpace)
        minSpace = spacing[i];
    }

    vspNode->SetEstimatedSampleDistance(minSpace * 0.5f);
  }
  else
    vspNode->SetEstimatedSampleDistance( 1.0f);
}

/*
 * return values:
 * -1: requested mapper not supported
 *  0: invalid input parameter
 *  1: success
 */
int vtkVolumeRenderingLogic::SetupMapperFromParametersNode(vtkMRMLVolumeRenderingParametersNode* vspNode)
{
  if (vspNode == NULL)
    return 0;

  this->Volume->SetMapper(NULL);
  this->EstimateSampleDistance(vspNode);

  switch(vspNode->GetCurrentVolumeMapper())//mapper specific initialization
  {
  case 0:
    this->MapperRaycast->SetInput( vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetImageData() );
    this->MapperRaycast->SetSampleDistance(vspNode->GetEstimatedSampleDistance());
    this->Volume->SetMapper(this->MapperRaycast);
    this->Volume->SetProperty(vspNode->GetVolumePropertyNode()->GetVolumeProperty());
    break;
  case 1:
    this->MapperGPURaycast->SetInput( vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetImageData() );
    this->MapperGPURaycast->SetFramerate(vspNode->GetExpectedFPS());
    if (this->MapperGPURaycast->IsRenderSupported(vspNode->GetVolumePropertyNode()->GetVolumeProperty()))
    {
      this->Volume->SetMapper(this->MapperGPURaycast);
      this->Volume->SetProperty(vspNode->GetVolumePropertyNode()->GetVolumeProperty());
    }
    else
      return -1;
    break;
  case 2:
    break;
  case 3:
    this->MapperTexture->SetInput( vtkMRMLScalarVolumeNode::SafeDownCast(vspNode->GetVolumeNode())->GetImageData() );
    this->MapperTexture->SetSampleDistance(vspNode->GetEstimatedSampleDistance());
    this->MapperTexture->SetFramerate(vspNode->GetExpectedFPS());
    if (this->MapperTexture->IsRenderSupported(vspNode->GetVolumePropertyNode()->GetVolumeProperty()))
    {
      this->Volume->SetMapper(this->MapperTexture);
      this->Volume->SetProperty(vspNode->GetVolumePropertyNode()->GetVolumeProperty());
    }
    else
      return -1;
    break;
  case 4:
    break;
  }

  this->SetExpectedFPS(vspNode);
  this->ComputeInternalVolumeSize(vspNode->GetGPUMemorySize());

  vtkMatrix4x4 *matrix = vtkMatrix4x4::New();
  this->CalculateMatrix(vspNode, matrix);
  this->Volume->PokeMatrix(matrix);
  matrix->Delete();

  return 1;
}

/* return values:
 * 0: cpu ray cast not used
 * 1: success
 */
int vtkVolumeRenderingLogic::SetupCPURayCastInteractive(vtkMRMLVolumeRenderingParametersNode* vspNode, int buttonDown)
{
  if (this->Volume->GetMapper() != this->MapperRaycast)
    return 0;

  //when start (rendering??) set CPU ray casting to be interactive
  if (buttonDown == 1)
  {
    float desiredTime = 1.0f/vspNode->GetExpectedFPS();//expected fps will not be 0 so safe to do division here

    this->MapperRaycast->SetAutoAdjustSampleDistances(1);
    this->MapperRaycast->ManualInteractiveOn();
    this->MapperRaycast->SetManualInteractiveRate(desiredTime);
  }
  else
  {
    //when end (rendering??) set CPU ray casting to be non-interactive high quality
    this->MapperRaycast->SetAutoAdjustSampleDistances(0);
    this->MapperRaycast->SetSampleDistance(vspNode->GetEstimatedSampleDistance());
    this->MapperRaycast->SetImageSampleDistance(1.0f);
    this->MapperRaycast->ManualInteractiveOff();
  }

  return 1;
}

void vtkVolumeRenderingLogic::SetVolumeVisibility(int isVisible)
{
  if (isVisible)
    this->Volume->VisibilityOn();
  else
    this->Volume->VisibilityOff();
}

//----------------------------------------------------------------------------
void vtkVolumeRenderingLogic::ProcessMRMLEvents(vtkObject *caller,
                                            unsigned long event,
                                            void *callData)
{
}


