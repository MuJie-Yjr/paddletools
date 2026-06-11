#pragma once

#include "domain/TrainingTask.h"

#include <QList>

namespace ppocr {

QList<TrainingTaskSpec> trainingTasks();
TrainingTaskSpec trainingTaskByKey(const QString& key);

}  // namespace ppocr
