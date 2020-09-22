#include "ModelLoading\ModelLoader.h"
#include "ModelLoading\Model.h"

ModelLoader::ModelLoader(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice)
	: TaskQueueThread(d3dDevice) {

}

Model* ModelLoader::loadModel(std::string name, std::string dir) {
	Model* loading = new Model(name, dir);
	enqueue(new ModelLoadTask(this, loading));
	return loading;
}
