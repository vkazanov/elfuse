(require 'elfuse)

(defvar hello-2--file-list '("." ".."))

(elfuse-define-op readdir (path)
  (message "READDIR: %s" path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-ENOENT))
  (apply 'vector hello-2--file-list))

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond ((equal path "/")
         (vector 'dir 0))
        ((member (file-name-nondirectory path) hello-2--file-list)
         (vector 'file 0))
        (t (signal 'elfuse-op-error elfuse-ENOENT))))

(elfuse-define-op create (path)
  (message "CREATE: %s" path)
  (add-to-list 'hello-2--file-list (file-name-nondirectory path))
  0)
