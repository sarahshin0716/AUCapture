import Foundation

enum AppGroup {
    static let identifier = "group.ai.fairytech.ios.monda"
    static let container: URL = {
        guard let container = FileManager().containerURL(forSecurityApplicationGroupIdentifier: identifier) else {
            preconditionFailure()
        }
        return container
    }()
}
