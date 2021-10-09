# Zombie Game AI Agent
Assignment made as part of my education at Howest DAE. The goal of this project was to implement an AI agent's behavior in a given zombie game framework. I am responsible for the implementations the main AI agent's behavior as well as the used steering behaviors. These are defined in the following files in the "project" folder:
- [States and Transitions](project/StatesAndTransitions.h)
- [Plugin](project/Plugin.cpp) + [Header](project/Plugin.h)
- [Steering Behaviors](project/SteeringBehaviors.cpp) + [Header](project/SteeringBehaviors.h)
- [Blended Steering](project/BlendedSteering.cpp) + [Header](project/BlendedSteering.h)

For more information, visit my website: https://bryncouvreur.wixsite.com/portfolio

#### Some notable functions:
- [GrabItemState::EvaluateItem](project/StatesAndTransitions.h#L172-L259)
- [EnterHouseState::Update](project/StatesAndTransitions.h#L74-L113)
- [Plugin::UseConsumables](project/Plugin.h#L302-L340)
